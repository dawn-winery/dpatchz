#include "patching.hpp"
#include "dwhbll-logging.hpp"

u64 Patcher::read(u8* buf, size_t size) {
    if (!buf || size == 0)
        return 0;

    ZSTD_outBuffer output = { buf, size, 0 };

    while (output.pos < output.size) {
        if (input.pos == input.size) {
            mem.read(reinterpret_cast<char*>(inBuf.data()), CHUNK_SIZE);
            std::streamsize readBytes = mem.gcount();

            if (readBytes == 0)
                throw std::runtime_error("Unexpected end of input");

            input.src = inBuf.data();
            input.size = static_cast<size_t>(readBytes);
            input.pos = 0;
        }

        size_t ret = ZSTD_decompressStream(dstream, &output, &input);
        if (ZSTD_isError(ret)) {
            throw std::runtime_error("ZSTD_decompressStream error: " + std::string(ZSTD_getErrorName(ret)));
        }

        if (ret == 0 && output.pos < output.size)
            throw std::runtime_error("Decompressed frame too small for requested size");
    }

    current_index += size;
    return size;
}

void Patcher::error(const std::string &err) const {
    if(cur_file)
        dwhbll::console::fatal("Error while patching {}: {}", cur_file->name, err);
    else
        dwhbll::console::fatal("Error while patching: {}", cur_file->name, err);
    std::exit(1);
}

std::filesystem::path get_tmp_dir(std::filesystem::path path) {
    // Use path/tmp if available
    if(!std::filesystem::exists(path / "tmp"))
        return path / "tmp";
    int cur = 1;
    // Otherwise use path/n.tmp, for the first n that is available
    while(std::filesystem::exists(path / (std::to_string(cur) + ".tmp"))) {
        cur++;
    }

    return path / (std::to_string(cur) + ".tmp");
}


void Patcher::merge_dirs(const std::filesystem::path& a, const std::filesystem::path& b) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(b)) {
        if (std::filesystem::is_directory(entry.path())) continue;

        std::filesystem::path relative_path = std::filesystem::relative(entry.path(), b);
        std::filesystem::path target_path = a / relative_path;

        try {
            std::filesystem::create_directories(target_path.parent_path());
            std::filesystem::rename(entry.path(), target_path);
        } catch (const std::exception& e) {
            error(std::format("Error copying {} to {}: {}", entry.path().string(), target_path.string(), e.what()));
        }
    }

    std::filesystem::remove_all(b);
}

void Patcher::patch(std::filesystem::path source, std::filesystem::path dest, bool inplace) {
    std::unique_ptr<VirtualFilesystemBuffer> read_buffer = std::make_unique<VirtualFilesystemBuffer>();
    for(const auto &file : diff.headData.oldFiles) {
        read_buffer->add_file(source / file.name);
    }
    CachedReader reader(std::move(read_buffer), cache_size);

    std::filesystem::path destionation_dir = dest;
    if(inplace) {
        dwhbll::console::info("Patching inplace");
        destionation_dir = get_tmp_dir(source);
    }

    for(const auto &dir : diff.headData.newDirs) {
        std::filesystem::create_directory(destionation_dir / dir.name);
    }

    auto& covers = diff.mainDiff.coverBuf.covers;

    u64 cover_idx = 0;
    i64 old_pos = 0;
    u64 to_read = covers[0].newPos;
    u64 written = 0;

    for(size_t i = 0; i < diff.headData.newFiles.size(); i++) {
        cur_file = &diff.headData.newFiles[i];
        std::filesystem::path destionation_file = destionation_dir / cur_file->name;
        if(inplace) {
            dwhbll::console::info("[{}/{}] Patching {} inplace", i + 1, diff.headData.newFiles.size(),
                                  (destionation_file).string());
        }
        else {
            dwhbll::console::info("[{}/{}] Patching {}", i + 1, diff.headData.newFiles.size(),
                                  (destionation_file).string());
        }
        std::ofstream f(destionation_file, std::ios::binary);

        while(written < cur_file->fileSize) {
            u64 remaining = cur_file->fileSize - written;

            if(to_read == 0 && cover_idx < covers.size()) {
                // Reading a cover
                CoverBuf::Cover& cov = covers[cover_idx];
                old_pos += cov.oldPos;

                u64 to_write = std::min(cov.length, remaining);
                std::vector<u8> v;
                if(!reader.seek(old_pos)) {
                    error("Error while seeking in vfs");
                }
                if(!reader.read_bytes(v, to_write)) {
                    error("Unexpected EOF");
                }
                assert(v.size() == to_write);
                if(!f.write(reinterpret_cast<char*>(v.data()), to_write)) {
                    error(std::format("Failed to write to {}", (destionation_file).string()));
                }

                written += to_write;
                old_pos += to_write;

                // I would assume that a cover will never go over file boundaries
                // but knowing how cursed this software is, that's not impossible
                if(remaining == to_write && remaining != cov.length) {
                    // We have written until the file end, but a part of the cover is still left
                    to_read = 0;
                    cov.length -= to_write;
                    cov.oldPos = 0;
                    cov.newPos = 0;
                }
                else {
                    // We have read the whole cover
                    if(cover_idx + 1 < covers.size())
                        to_read = covers[cover_idx + 1].newPos;
                    cover_idx++;
                }
            }
            else {
                u64 to_write = remaining;
                if(cover_idx < covers.size()) {
                    to_write = std::min(remaining, to_read);
                }
                std::vector<u8> v(to_write);
                read(v.data(), to_write);
                f.write(reinterpret_cast<char*>(v.data()), to_write);
                to_read -= to_write;
                written += to_write;
            }
        }

        written = 0;
        dwhbll::console::info("Successfully patched {} [{}/{}]", (dest / cur_file->name).string(), 
                              i + 1, diff.headData.newFiles.size());

        if(inplace) {
            merge_dirs(source, destionation_dir);
        }
    }

    dwhbll::console::info("Everything patched with success (hopefully)");
}
