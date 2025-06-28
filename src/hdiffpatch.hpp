#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <vector>

struct VarInt {
    uint64_t value;
    
    static VarInt parse(std::ifstream input) {
        uint8_t b;
        uint64_t res;
        input.read(reinterpret_cast<char*>(&b), 1);
        res = b & 0x7f;

        while(b & 0x80) {
            input.read(reinterpret_cast<char*>(&b), 1);
            res = (res << 7) | (b & 0x7f);
        }

        return VarInt { res };
    }
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
    VarInt sameFilePairCount;
    VarInt sameFileSize;
    VarInt newExecuteCount;
    VarInt privateReservedDataSize;
    VarInt privateExternDataSize;
    VarInt externDataSize;
    
    VarInt headDataSize;
    VarInt headDataCompressedSize;
    VarInt checksumByteSize;

    // size: checksumByteSize
    std::vector<uint8_t> checksum;

    // These 2 are most likely not needed, from testing they always have size 0
    // size: privateExternDataSize
    std::vector<uint8_t> privateExternalData;
    // size: externDataSize
    std::vector<uint8_t> externData;
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
};

struct HeadData {
    // size: oldPathCount
    std::vector<std::string> oldFiles;
    // size: newPathCount
    std::vector<std::string> newFiles;

    // size: oldRefFileCount
    std::vector<uint8_t> oldFileOffsets;
    // size: newRefFileCount
    std::vector<uint8_t> newFileOffsets;

    // size: oldRefFileCount
    std::vector<VarInt> oldFileSizes;
    // size: newRefFileCount
    std::vector<VarInt> newFileSizes;

    // Checksums?
    // size: newRefFileCount
    std::vector<VarInt> unknown;
};
