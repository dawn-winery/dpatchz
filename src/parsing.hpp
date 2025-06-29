#pragma once

#include "utils.hpp"
#include <memory>
#include <string>
#include <vector>

class Parser;

/*
 * Headers that roughly describe the structure of a hdiffz diff file
 * Some fields are left out because not used in parsing (or we know kuro sets them to 0)
 */
struct VarInt {
    union {
        i64 value_signed;
        u64 value;
    };
};

struct CoverBuf {
    struct Cover {
        i64 oldPos;
        u64 newPos;
        u64 length;
    };

    std::vector<Cover> covers;

    static CoverBuf parse(Parser& parser, u64 size, 
                         u64 compressed_size, u64 covert_count);
    std::string to_string();
};

struct DiffZ {
    std::string compressType;

    VarInt newDataSize, oldDataSize, coverCount;
    
    VarInt coverBufSize, compressedCoverBufSize;
    VarInt rleCtrlBufSize, compressedRleCtrlBufSize;
    VarInt rleCodeBufSize, compressedRleCodeBufSize;
    VarInt newDataDiffSize, compressedNewDataDiffSize;

    CoverBuf coverBuf;

    static DiffZ parse(Parser& parser);
    std::string to_string();
};

struct File {
    std::string name;

    u8 fileOffset;
    u64 fileSize;
};

struct Directory {
    std::string name;
};

struct HeadData {
    std::vector<File> oldFiles;
    std::vector<File> newFiles;
    std::vector<Directory> oldDirs;
    std::vector<Directory> newDirs;

    static HeadData parse(Parser& parser, u64 size, u64 compressed_size, 
                          u64 old_path_count, u64 new_path_count,
                          u64 old_ref_file_count, u64 new_ref_file_count);
    std::string to_string();
};

struct DirDiff {
    std::string compressionType, checksumType;

    bool oldPathIsDir, newPathIsDir;

    VarInt oldPathCount, newPathCount;
    VarInt oldPathSumSize, newPathSumSize;
    VarInt oldRefFileCount, newRefFileCount;
    VarInt oldRefSize, newRefSize;

    // Supposedly always equal to 0
    // VarInt sameFilePairCount;
    // VarInt sameFileSize;
    // VarInt newExecuteCount;
    // VarInt privateReservedDataSize;
    // VarInt privateExternDataSize;
    // VarInt externDataSize;
    
    VarInt headDataSize, headDataCompressedSize;
    VarInt checksumByteSize;

    // size: checksumByteSize
    std::vector<u8> checksum;

    HeadData headData;
    DiffZ mainDiff;

    static DirDiff parse(Parser& parser);
    std::string to_string();
};

class Parser {
private:
    std::istream* stream_;  // Changed from reference to pointer
    std::unique_ptr<VectorIStream> owned_stream_;  // For sub-parsers
    std::string context_;

    [[noreturn]] void error(const std::string& message);
    std::string format_context() const;

public:
    explicit Parser(std::istream &stream, std::string context = "")
        : stream_(&stream), context_(context) {}

    explicit Parser(std::unique_ptr<VectorIStream> stream, std::string context = "")
        : stream_(stream.get()), owned_stream_(std::move(stream)), context_(context) {}

    std::streampos position() const { return stream_->tellg(); }

    // Ensures that exactly `b` bytes have been read, otherwise errors out
    void check_read(u64 b);

    std::vector<u8> read_maybe_compressed(u64 size, u64 compressed_size);
    Parser sub_parser(const std::vector<u8>& data, const std::string& sub_context);

    VarInt read_varint(u8 kTagBit = 0);
    void match_varint(u64 expected);

    template <Byte T>
    T read() {
        return read_bytes<T>(1)[0];
    }

    template <Byte T>
    std::vector<T> read_bytes(size_t n) {
        std::vector<T> buffer(n);
        if(!stream_->read(reinterpret_cast<char*>(buffer.data()), n))
            error(std::format("Unexpected EOF while reading {} bytes", n));

        return buffer;
    }

    // Consumes c
    template <Byte T = u8, typename Container = std::vector<T>>
    Container read_until(T c) {
        Container buffer;
        T b;
        while(true) {
            if(!stream_->read(reinterpret_cast<char*>(&b), 1))
                error(std::format("Unexpected EOF while reading until 0x{:02X}", c));

            if(b != c)
                buffer.push_back(b);
            else 
                break;
        }

        return buffer;
    }

    template <Byte T>
    void match(T expected) {
        T r = read<T>();
        if(r != expected)
            error(std::format("Expected {}, got {}", 
                              static_cast<int>(expected), 
                              static_cast<int>(r)));
    }

    template <Byte T>
    void match_bytes(const T* expected, size_t size) {
        auto bytes = read_bytes<T>(size);
        if(std::memcmp(bytes.data(), expected, size) != 0) {
            error(std::format("Expected '{}', got '{}'", 
                              format_bytes(expected, size), 
                              format_bytes(bytes.data(), size)));
        }
    }
};
