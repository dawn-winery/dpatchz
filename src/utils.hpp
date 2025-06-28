#pragma once

#include "logging.hpp"

#include <iomanip>
#include <vector>
#include <sstream>
#include <cstring>
#include <cassert>

#define MALFORMED(file, fmt, ...)                                       \
    do {                                                                \
        dwhbll::console::fatal("Malformed diff file at offset 0x{:X}",  \
                               static_cast<size_t>(file.tellg()));      \
        dwhbll::console::fatal(fmt, ##__VA_ARGS__);                     \
        exit(1);                                                        \
    } while(0)

#define MATCH_BYTES(file, cmp)                                                      \
    do {                                                                            \
        auto bytes = read_bytes(file, sizeof(cmp) - 1);                             \
        if(memcmp(bytes.data(), cmp, sizeof(cmp) - 1) != 0)                         \
            MALFORMED(file, "MATCH_BYTES failed:\n Expected: '{}'\n Actual: '{}'",  \
                      cmp, format_bytes(bytes.data(), sizeof(cmp) - 1));            \
    } while(0)

#define MATCH_UNTIL(file, target, cmp)                                              \
    do {                                                                            \
        auto bytes = read_until(file, target);                                      \
        if(memcmp(bytes.data(), cmp, bytes.size()) != 0)                            \
            MALFORMED(file, "MATCH_UNTIL failed:\n Expected: '{}'\n Actual: '{}'",  \
                      cmp, format_bytes(bytes.data(), bytes.size()));               \
    } while(0)

#define MATCH_VARINT(file, check)                                                   \
    do {                                                                            \
        VarInt v = VarInt::parse(file);                                             \
        if(v.value != check)                                                        \
            MALFORMED(file, "MATCH_VARINT failed:\n Expected: '{}'\n Actual: '{}'", \
                      check, v.value);                                              \
    } while(0)

inline std::string format_bytes(const uint8_t* data, size_t n) {
    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

inline std::vector<uint8_t> read_bytes(std::istream &file, size_t N) {
    std::vector<uint8_t> buffer(N);
    if(!file.read(reinterpret_cast<char*>(buffer.data()), N))
        MALFORMED(file, "Unexpected EOF while reading {} bytes", N);

    return buffer;
}

template <typename Container = std::vector<uint8_t>>
inline Container read_until(std::istream &file, uint8_t target) {
    Container buffer;
    while(1) {
        int next = file.peek();
        if(next == EOF || static_cast<uint8_t>(next) == target)
            break;

        char byte;
        file.get(byte);

        if constexpr (std::is_same<Container, std::string>())
            buffer.push_back(byte);
        else 
            buffer.push_back(static_cast<uint8_t>(byte));
    }

    if(file.peek())
        MALFORMED(file, "Unexpected EOF while reading until {:c}", target);

    return buffer;
}
