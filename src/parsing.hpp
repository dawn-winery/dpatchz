#pragma once

#include <cstdint>
#include <string>
#include <vector>

/*
 * Headers that roughly describe the structure of a hdiffz diff file
 * Some fields are left out because not used in parsing (or we know kuro sets them to 0)
 */
struct VarInt {
    uint64_t value;
    
    static VarInt parse(std::istream &input, uint8_t kTagBit = 0);
};

struct CoverBuf {
    struct Cover {
        uint64_t oldPos;
        uint64_t newPos;
        uint64_t length;
    };

    std::vector<Cover> covers;

    static CoverBuf parse(std::istream& file, uint64_t compressed_size, 
                         uint64_t size, uint64_t covert_count);
    std::string to_string();
};

struct DiffZ {
    std::string compressType;

    VarInt newDataSize;
    VarInt oldDataSize;
    VarInt coverCount;
    
    VarInt coverBufSize;
    VarInt compressedCoverBufSize;
    VarInt rleCtrlBufSize;
    VarInt compressedRleCtrlBufSize;
    VarInt rleCodeBufSize;
    VarInt compressedRleCodeBufSize;
    VarInt newDataDiffSize;
    VarInt compressedNewDataDiffSize;

    CoverBuf coverBuf;

    static DiffZ parse(std::istream& file);
    std::string to_string();
};

struct File {
    std::string oldName;
    std::string newName;

    uint8_t oldFileOffset;
    uint8_t newFileOffset;

    uint64_t oldFileSize;
    uint64_t newFileSize;
};

struct Directory {
    std::string oldName;
    std::string newName;
};

struct HeadData {
    std::vector<File> files;
    std::vector<Directory> dirs;

    static HeadData parse(std::istream& file, uint64_t size, uint64_t compressed_size, 
                          uint64_t old_path_count, uint64_t new_path_count,
                          uint64_t old_ref_file_count, uint64_t new_ref_file_count);
    std::string to_string();
};

struct DirDiff {
    std::string compressionType;
    std::string checksumType;

    bool oldPathIsDir;
    bool newPathIsDir;

    VarInt oldPathCount;
    VarInt oldPathSumSize;
    VarInt newPathCount;
    VarInt newPathSumSize;
    VarInt oldRefFileCount;
    VarInt oldRefSize;
    VarInt newRefFileCount;
    VarInt newRefSize;

    // Supposedly always equal to 0
    // VarInt sameFilePairCount;
    // VarInt sameFileSize;
    // VarInt newExecuteCount;
    // VarInt privateReservedDataSize;
    // VarInt privateExternDataSize;
    // VarInt externDataSize;
    
    VarInt headDataSize;
    VarInt headDataCompressedSize;
    VarInt checksumByteSize;

    // size: checksumByteSize
    std::vector<uint8_t> checksum;

    HeadData headData;
    DiffZ mainDiff;

    static DirDiff parse(std::istream& file);
    std::string to_string();
};
