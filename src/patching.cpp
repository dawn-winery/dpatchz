#include "patching.hpp"
#include "dwhbll-logging.hpp"
#include <utility>

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
    if(cur_out_file)
        dwhbll::console::fatal("Error while patching {}: {}", cur_out_file->name, err);
    else
        dwhbll::console::fatal("Error while patching: {}", cur_out_file->name, err);
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

void Patcher::update_file(u64 offset) {
    u64 cur_offset = 0;
    for(auto &file : diff.headData.oldFiles) {
        if(cur_offset + file.fileSize >= offset) {
            if(file.name != cur_in_file.file->name) {
                // Update the file pointer
                fclose(cur_in_file.ptr);
                cur_in_file.file = &file;
                cur_in_file.ptr = fopen((source / file.name).c_str(), "r");
                if(!cur_in_file.ptr) {
                    error(std::format("Failed to open file: {} ({})", 
                                      (source / file.name).string(), strerror(errno)));
                }
            }
            fseek(cur_in_file.ptr, offset - cur_offset, SEEK_SET);
            return;
        }
        cur_offset += file.fileSize;
    }
    std::unreachable();
}

void Patcher::patch(bool inplace) {
    cur_in_file.file = &diff.headData.oldFiles[0];
    cur_in_file.ptr = fopen((source / cur_in_file.file->name).c_str(), "r");
    if(!cur_in_file.ptr) {
        error(std::format("Failed to open file: {} ({})", 
                          (source / cur_in_file.file->name).string(), 
                          strerror(errno)));
    }

    std::filesystem::path destionation_dir = dest;
    if(inplace) {
        destionation_dir = get_tmp_dir(source);
        dwhbll::console::info("Patching inplace to {} (temporary dir)", destionation_dir.string());
        std::filesystem::create_directory(destionation_dir.string());
    }

    for(const auto &dir : diff.headData.newDirs) {
        std::filesystem::create_directories(destionation_dir / dir.name);
    }

    auto& covers = diff.mainDiff.coverBuf.covers;

    u64 cover_idx = 0;
    i64 old_pos = 0;
    u64 read_from_new_data = covers[0].newPos;

    for(size_t i = 0; i < diff.headData.newFiles.size(); i++) {
        u64 written = 0;

        cur_out_file = &diff.headData.newFiles[i];
        std::filesystem::path destionation_file = destionation_dir / cur_out_file->name;
        if(inplace) {
            dwhbll::console::info("[{}/{}] Patching {} inplace", i + 1, diff.headData.newFiles.size(),
                                  (destionation_file).string());
        }
        else {
            dwhbll::console::info("[{}/{}] Patching {}", i + 1, diff.headData.newFiles.size(),
                                  (destionation_file).string());
        }

        FILE* cur = fopen(destionation_file.c_str(), "w");
        if(!cur)
            error(std::format("Error opening file: {} ({})", destionation_file.string(),
                              strerror(errno)));

        while(written < cur_out_file->fileSize) {
            u64 remaining = cur_out_file->fileSize - written;

            if(read_from_new_data == 0 && cover_idx < covers.size()) {
                // Reading a cover
                CoverBuf::Cover& cov = covers[cover_idx];
                old_pos += cov.oldPos;

                u64 to_write = std::min(cov.length, remaining);

                update_file(old_pos);
                if(copy_file_range(fileno(cur_in_file.ptr), nullptr, fileno(cur), 
                                   nullptr, to_write, 0) != to_write) {
                    error(std::format("Failed to copy data from {} to {} ({})",
                                      (source / cur_in_file.file->name).string(),
                                      destionation_file.string(), strerror(errno)));
                }
                if(fflush(cur) != 0) {
                    error(std::format("Failed to flush file stream: {} ({})", 
                                      destionation_file.string(), strerror(errno)));
                }

                written += to_write;
                old_pos += to_write;

                // I would assume that a cover will never go over file boundaries
                // but knowing how cursed this software is, that's not impossible
                if(remaining == to_write && remaining != cov.length) {
                    // We have written until the file end, but a part of the cover is still left
                    read_from_new_data = 0;
                    cov.length -= to_write;
                    cov.oldPos = 0;
                    cov.newPos = 0;
                }
                else {
                    // We have read the whole cover
                    if(cover_idx + 1 < covers.size())
                        read_from_new_data = covers[cover_idx + 1].newPos;
                    cover_idx++;
                }
            }
            else {
                u64 to_write = remaining;
                if(cover_idx < covers.size()) {
                    to_write = std::min(remaining, read_from_new_data);
                }
                std::vector<u8> v(to_write);
                read(v.data(), to_write);
                if(fwrite(reinterpret_cast<char*>(v.data()), 1, to_write, cur) != to_write) {
                    error(std::format("Failed to write to file: {} ({})", 
                                      destionation_file.string(), strerror(errno)));
                }
                if(fflush(cur) != 0) {
                    error(std::format("Failed to flush file stream: {} ({})", 
                                      destionation_file.string(), strerror(errno)));
                }
                read_from_new_data -= to_write;
                written += to_write;
            }
        }

        dwhbll::console::info("[{}/{}] Patched {}", i + 1, 
                              diff.headData.newFiles.size(), 
                              (dest / cur_out_file->name).string());
    }

    if(inplace) {
        dwhbll::console::info("Merging temporary directory {} with {}", 
                              destionation_dir.string(), source.string());
        merge_dirs(source, destionation_dir);
    }

    dwhbll::console::info("Everything patched with success (hopefully)");
}
