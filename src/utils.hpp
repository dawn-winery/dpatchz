#pragma once

#include <iomanip>
#include <sstream>
#include <cstring>
#include <cassert>
#include <zstd.h>

typedef uint8_t u8;
typedef uint64_t u64;
typedef int64_t i64;

template <typename T>
concept Byte = std::is_same_v<T, u8> || std::is_same_v<T, char>;

static std::size_t ZSTD_INPUT_BUFFER_SIZE = ZSTD_DStreamInSize();
static std::size_t ZSTD_OUTPUT_BUFFER_SIZE = ZSTD_DStreamOutSize();

template <Byte T>
std::string format_bytes(const T* data, size_t n) {
    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}
