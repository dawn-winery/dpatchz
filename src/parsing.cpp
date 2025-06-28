#include "parsing.hpp"
#include "utils.hpp"

#include <string>

VarInt VarInt::parse(std::ifstream &input) {
    uint8_t b;
    uint64_t res;
    if(!input.read(reinterpret_cast<char*>(&b), 1))
        MALFORMED(input, "Unexpected EOF while reading VarInt");
    res = b & 0x7f;

    while(b & 0x80) {
        if(!input.read(reinterpret_cast<char*>(&b), 1))
            MALFORMED(input, "Unexpected EOF while reading VarInt");
        res = (res << 7) | (b & 0x7f);
    }

    return VarInt { res };
}

DirDiff DirDiff::parse(std::ifstream& file) {
    DirDiff diff;

    // Hardcoded because why not
    // HDIFF19 & compressionType & checksumType \0 oldPathIsDir newPathIsDir
    MATCH_BYTES(file, "HDIFF19&zstd&fadler64\0\1\1");
    diff.compressionType = "zstd";
    diff.checksumType = "fadler64";

    diff.oldPathCount = VarInt::parse(file);
    diff.oldPathSumSize = VarInt::parse(file);
    diff.newPathCount = VarInt::parse(file);
    diff.newPathSumSize = VarInt::parse(file);
    diff.oldRefFileCount = VarInt::parse(file);
    diff.oldRefSize = VarInt::parse(file);
    diff.newRefFileCount = VarInt::parse(file);
    diff.newRefSize = VarInt::parse(file);

    // We discard all this since it seems to always be 0 in kuro diffs
    // We still check it's still really zero, just in case kuro changes the diffs
    // sameFilePairCount
    MATCH_VARINT(file, 0);
    // sameFileSize
    MATCH_VARINT(file, 0);
    // newExecuteCount
    MATCH_VARINT(file, 0);
    // privateReservedDataSize
    MATCH_VARINT(file, 0);
    // privateExternDataSize
    MATCH_VARINT(file, 0);
    // externDataSize
    MATCH_VARINT(file, 0);
    
    diff.headDataSize = VarInt::parse(file);
    diff.headDataCompressedSize = VarInt::parse(file);
    diff.checksumByteSize = VarInt::parse(file);

    return diff;
}

std::string DirDiff::to_string() {
    std::string s = std::format(
        "DirDiff {{\n"
        "  compressionType: {}\n"
        "  checksumType: {}\n"
        "  oldPathIsDir: {}\n"
        "  newPathIsDir: {}\n"
        "  oldPathCount: {}\n"
        "  oldPathSumSize: {}\n"
        "  newPathCount: {}\n"
        "  newPathSumSize: {}\n"
        "  oldRefFileCount: {}\n"
        "  oldRefSize: {}\n"
        "  newRefFileCount: {}\n"
        "  newRefSize: {}\n"
        "  headDataSize: {}\n"
        "  headDataCompressedSize: {}\n"
        "  checksumByteSize: {}\n"
        "  checksum: {}\n"
        "}}",
        compressionType,
        checksumType,
        oldPathIsDir,
        newPathIsDir,
        oldPathCount.value,
        oldPathSumSize.value,
        newPathCount.value,
        newPathSumSize.value,
        oldRefFileCount.value,
        oldRefSize.value,
        newRefFileCount.value,
        newRefSize.value,
        headDataSize.value,
        headDataCompressedSize.value,
        checksumByteSize.value,
        format_bytes(checksum.data(), checksumByteSize.value)
    );

    return s;
}   
