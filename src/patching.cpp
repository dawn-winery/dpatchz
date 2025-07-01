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

void Patcher::patch(std::filesystem::path source, std::filesystem::path dest) {
    std::unique_ptr<VirtualFilesystemBuffer> read_buffer = std::make_unique<VirtualFilesystemBuffer>();
    for(auto file : diff.headData.oldFiles) {
        read_buffer->add_file(source / file.name);
    }
    CachedReader reader(std::move(read_buffer));

    for(auto dir : diff.headData.newDirs) {
        std::filesystem::create_directory(dest / dir.name);
    }

    auto& covers = diff.mainDiff.coverBuf.covers;

    u64 cover_idx = 0;
    i64 old_pos = 0;
    // u64 new_pos = covers[0].newPos;
    u64 to_read = covers[0].newPos;
    u64 written = 0;

    for(size_t i = 0; i < diff.headData.newFiles.size(); i++) {
        cur_file = &diff.headData.newFiles[i];
        dwhbll::console::info("Patching {} [{}/{}]", (dest / cur_file->name).string(), 
                              i + 1, diff.headData.newFiles.size());
        std::ofstream f(dest / cur_file->name, std::ios::binary);
        while(written < cur_file->fileSize) {
            u64 remaining = cur_file->fileSize - written;

            if(to_read == 0 && cover_idx < covers.size()) {
                // Reading a cover
                CoverBuf::Cover& cov = covers[cover_idx];
                old_pos += cov.oldPos;
                // new_pos += cov.newPos;

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
                    error(std::format("Failed to write to {}", (dest / cur_file->name).string()));
                }

                written += to_write;
                old_pos += to_write;

                // I would assume that a cover will never go over file boundaries
                // but knowing how cursed this software is, that's not impossible
                if(remaining == to_write && remaining != cov.length) {
                    // We have written until the file end, but a part of the cover is still left
                    to_read = 0;
                    // new_pos += to_write;
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
    }
    dwhbll::console::info("Everything patched with success (hopefully)");
}
