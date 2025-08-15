#pragma once

#include "parsing.hpp"

#include <format>

static size_t CHUNK_SIZE = ZSTD_DStreamInSize();

struct File {
    FILE* ptr;
    DiffFile* file;
};

class Patcher {
private:
    std::filesystem::path source;
    std::filesystem::path dest;

    DirDiff diff;
    std::ifstream mem;
    u64 current_index = 0;
    File cur_in_file;
    DiffFile* cur_out_file;

    ZSTD_DStream* dstream = nullptr;
    std::vector<u8> inBuf;
    ZSTD_inBuffer input = { nullptr, 0, 0 };

    u64 read(u8* buf, size_t size);

    [[noreturn]] void error(const std::string& message) const;
    void merge_dirs(const std::filesystem::path& a, const std::filesystem::path& b);

    void update_file(u64 offset);

public:
    explicit Patcher(DirDiff diff, std::filesystem::path diff_file, 
                     std::filesystem::path source_, std::filesystem::path dest_)
        : diff(diff), mem(diff_file), inBuf(CHUNK_SIZE), source(source_), dest(dest_) {
        if (!mem)
            error(std::format("Failed to open diff file {}", diff_file.string()));

        mem.seekg(diff.mainDiff.newDataOffset, std::ios::beg);

        dstream = ZSTD_createDStream();
        if (!dstream)
            error("Failed to create ZSTD_DStream");

        size_t const initResult = ZSTD_initDStream(dstream);
        if (ZSTD_isError(initResult))
            error("ZSTD_initDStream error");
    }

    ~Patcher() {
        if (dstream)
            ZSTD_freeDStream(dstream);
    }

    void patch(bool inplace);
};
