#include "parsing.hpp"

#include "utils.hpp"

#include <zstd.h>
#include <string>

// Ugly thing to convert std::vector<uint8_t> to an std::istream
struct membuf: std::streambuf {
    membuf(char const* base, size_t size) {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
};
struct imemstream: virtual membuf, std::istream {
    imemstream(char const* base, size_t size)
        : membuf(base, size)
        , std::istream(static_cast<std::streambuf*>(this)) {
    }
};

VarInt VarInt::parse(std::istream &input) {
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

DiffZ DiffZ::parse(std::istream& file) {
    DiffZ diff;

    MATCH_BYTES(file, "HDIFF13&zstd\0");
    diff.compressType = "zstd";

    diff.newDataSize = VarInt::parse(file);
    diff.oldDataSize = VarInt::parse(file);
    diff.coverCount = VarInt::parse(file);
    diff.coverBufSize = VarInt::parse(file);
    diff.compressedCoverBufSize = VarInt::parse(file);
    diff.rleCtrlBufSize = VarInt::parse(file);
    diff.compressedRleCtrlBufSize = VarInt::parse(file);
    diff.rleCodeBufSize = VarInt::parse(file);
    diff.compressedRleCodeBufSize = VarInt::parse(file);
    diff.newDataDiffSize = VarInt::parse(file);
    diff.compressedNewDataDiffSize = VarInt::parse(file);

    return diff;
}

std::string DiffZ::to_string() {
    std::string s = std::format(
        "DiffZ {{\n"
        "  compressType: {}\n"
        "  newDataSize: {}\n"
        "  oldDataSize: {}\n"
        "  coverCount: {}\n"
        "  coverBufSize: {}\n"
        "  compressedCoverBufSize: {}\n"
        "  rleCtrlBufSize: {}\n"
        "  compressedRleCtrlBufSize: {}\n"
        "  rleCodeBufSize: {}\n"
        "  compressedRleCodeBufSize: {}\n"
        "  newDataDiffSize: {}\n"
        "  compressedNewDataDiffSize: {}\n"
        "}}",
        compressType,
        newDataSize.value,
        oldDataSize.value,
        coverCount.value,
        coverBufSize.value,
        compressedCoverBufSize.value,
        rleCtrlBufSize.value,
        compressedRleCtrlBufSize.value,
        rleCodeBufSize.value,
        compressedRleCodeBufSize.value,
        newDataDiffSize.value,
        compressedNewDataDiffSize.value
    );

    return s;
}

HeadData HeadData::parse(std::istream& file, uint64_t size, uint64_t compressed_size,
                          uint64_t old_path_count, uint64_t new_path_count,
                          uint64_t old_ref_file_count, uint64_t new_ref_file_count) {
    HeadData head;

    // Assume no files were deleted or created
    // TODO: don't
    assert(old_path_count == new_path_count);
    assert(old_ref_file_count == new_ref_file_count);

    std::vector<uint8_t> data;
    // Data is not compressed
    if(compressed_size > 0) {
        std::vector<uint8_t> compressed_data = read_bytes(file, compressed_size);
        uint64_t decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_size);
        if(decompressed_size == ZSTD_CONTENTSIZE_ERROR 
            || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN
            || decompressed_size != size) {
            MALFORMED(file, "Failed to get HeadData uncompressed size");
        }

        data.resize(decompressed_size);
        size_t result = ZSTD_decompress(data.data(), decompressed_size, compressed_data.data(),
                                        compressed_size);
        if(ZSTD_isError(result)) {
            MALFORMED(file, "ZSTD_decompress on HeadData failed");
            exit(1);
        }
    }
    // Data is not compressed
    else {
        data = read_bytes(file, size);
    }

    imemstream data_stream(reinterpret_cast<char*>(data.data()), data.size());

    std::vector<std::string> oldFiles;
    std::vector<std::string> newFiles;

    std::vector<uint8_t> oldFileOffsets;
    std::vector<uint8_t> newFileOffsets;

    std::vector<VarInt> oldFileSizes;
    std::vector<VarInt> newFileSizes;

    // Checksums?
    // std::vector<VarInt> unknown;

    for(size_t i = 0; i < old_path_count; i++) {
        oldFiles.push_back(read_until<std::string>(data_stream, '\0'));
        MATCH_BYTES(data_stream, "\0");
    }
    for(size_t i = 0; i < new_path_count; i++) {
        newFiles.push_back(read_until<std::string>(data_stream, '\0'));
        MATCH_BYTES(data_stream, "\0");
    }

    for(size_t i = 0; i < old_ref_file_count; i++)
        oldFileOffsets.push_back(read_bytes(data_stream, 1)[0]);
    for(size_t i = 0; i < new_ref_file_count; i++)
        newFileOffsets.push_back(read_bytes(data_stream, 1)[0]);

    for(size_t i = 0; i < old_ref_file_count; i++)
        oldFileSizes.push_back(VarInt::parse(data_stream));
    for(size_t i = 0; i < new_ref_file_count; i++)
        newFileSizes.push_back(VarInt::parse(data_stream));

    for(size_t i = 0, j = 0; i < old_path_count; i++) {
        if(oldFiles[i].ends_with('/') || oldFiles[i].empty()) {
            head.dirs.push_back(Directory(oldFiles[i], newFiles[i]));
        }
        else {
            head.files.push_back(File(oldFiles[i], newFiles[i], 
                                      oldFileOffsets[j], newFileOffsets[j],
                                      oldFileSizes[j].value, newFileSizes[j].value));
            j++;
        }
    }

    assert(head.files.size() == old_ref_file_count);
    assert(head.dirs.size() == old_path_count - old_ref_file_count);

    return head;
}

std::string HeadData::to_string() {
    std::ostringstream oss;
    oss << "HeadData {\n";

    oss << "  files: [\n";
    for (const auto& file : files) {
        oss << "    {\n"
            << "      oldName: \"" << file.oldName << "\",\n"
            << "      newName: \"" << file.newName << "\",\n"
            << "      oldFileOffset: " << static_cast<int>(file.oldFileOffset) << ",\n"
            << "      newFileOffset: " << static_cast<int>(file.newFileOffset) << ",\n"
            << "      oldFileSize: " << file.oldFileSize << ",\n"
            << "      newFileSize: " << file.newFileSize << "\n"
            << "    },\n";
    }
    oss << "  ],\n";

    oss << "  dirs: [\n";
    for (const auto& dir : dirs) {
        oss << "    {\n"
            << "      oldName: \"" << dir.oldName << "\",\n"
            << "      newName: \"" << dir.newName << "\"\n"
            << "    },\n";
    }
    oss << "  ]\n";

    oss << "}";
    return oss.str();
}

DirDiff DirDiff::parse(std::istream& file) {
    DirDiff diff;

    // Hardcoded because they shouldn't change in foreseeable future
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

    diff.checksum = read_bytes(file, diff.checksumByteSize.value * 4);
    diff.headData = HeadData::parse(file, diff.headDataSize.value, diff.headDataCompressedSize.value, 
                                    diff.oldPathCount.value, diff.newPathCount.value,
                                    diff.oldRefFileCount.value, diff.newRefFileCount.value);
    diff.mainDiff = DiffZ::parse(file);

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
        format_bytes(checksum.data(), checksumByteSize.value * 4)
    );

    return s;
}   
