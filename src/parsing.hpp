#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <vector>

/*
 * Headers that roughly describe the structure of a hdiffz diff file
 * Some fields are not left out because not used in parsing (or we know kuro sets them to 0)
 */
struct VarInt {
    uint64_t value;
    
    static VarInt parse(std::ifstream &input);
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

    static DirDiff parse(std::ifstream& file);
    std::string to_string();
};
