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

struct VectorStreamBuf : std::streambuf {
    VectorStreamBuf(const u8* data, size_t size) {
        char* p = const_cast<char*>(reinterpret_cast<const char*>(data));
        setg(p, p, p + size);
    }

    // Overrides needed for tellg() to work properly
    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                          std::ios_base::openmode which = std::ios_base::in) override {
        if (which != std::ios_base::in) return -1;
        
        char* new_pos = nullptr;
        switch (way) {
            case std::ios_base::beg:
                new_pos = eback() + off;
                break;
            case std::ios_base::cur:
                new_pos = gptr() + off;
                break;
            case std::ios_base::end:
                new_pos = egptr() + off;
                break;
            default:
                return -1;
        }
        
        if (new_pos < eback() || new_pos > egptr()) return -1;
        
        setg(eback(), new_pos, egptr());
        return new_pos - eback();
    }
    
    std::streampos seekpos(std::streampos sp,
                          std::ios_base::openmode which = std::ios_base::in) override {
        return seekoff(sp, std::ios_base::beg, which);
    }
};

class VectorIStream : public std::istream {
public:
    VectorIStream(const u8* data, size_t size)
        : std::istream(nullptr), buf_(data, size) {
        rdbuf(&buf_);
    }

private:
    VectorStreamBuf buf_;
};

template <Byte T>
std::string format_bytes(const T* data, size_t n) {
    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}
