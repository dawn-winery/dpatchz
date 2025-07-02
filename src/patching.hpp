#pragma once

#include "parsing.hpp"

static size_t CHUNK_SIZE = ZSTD_DStreamInSize();

class Patcher {
private:
    DirDiff diff;
    std::ifstream mem;
    u64 current_index = 0;
    File* cur_file;

    ZSTD_DStream* dstream = nullptr;
    std::vector<u8> inBuf;
    ZSTD_inBuffer input = { nullptr, 0, 0 };

    u64 read(u8* buf, size_t size);

    [[noreturn]] void error(const std::string& message) const;
    void merge_dirs(const std::filesystem::path& a, const std::filesystem::path& b);

public:
    explicit Patcher(DirDiff diff, std::filesystem::path diff_file)
        : diff(diff), mem(diff_file), inBuf(CHUNK_SIZE) {
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

    void patch(std::filesystem::path source, std::filesystem::path dest, bool inplace);
};
