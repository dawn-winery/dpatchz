#include "parsing.hpp"

#include "logging.hpp"
#include "utils.hpp"

#include <zstd.h>
#include <string>

CoverBuf CoverBuf::parse(Parser& parser, u64 size, 
                         u64 compressed_size, u64 covert_count) {
    CoverBuf buf;

    std::vector<u8> data = parser.read_maybe_compressed(size, compressed_size);
    Parser sub_parser = parser.sub_parser(data, "cover_buf");

    assert(data.size() == size);

    for(size_t i = 0; i < covert_count; i++) {
        Cover cover;
        cover.oldPos = sub_parser.read_varint(1).value_signed;
        cover.newPos = sub_parser.read_varint().value;
        cover.length = sub_parser.read_varint().value;
        buf.covers.push_back(cover);
    }

    // If it hasn't read exactly `size` bytes, something has gone wrong
    sub_parser.check_read(size);

    return buf;
}

std::string CoverBuf::to_string() {
    std::ostringstream s;
    s << "CoverBuf [\n";
    for(auto cover : covers) {
        s << "  {";
        s << " oldPos: " << cover.oldPos;
        s << "  newPos: " << cover.newPos;
        s << "  length: " << cover.length;
        s << " },\n";
    }
    s << "]";
    return s.str();
}

DiffZ DiffZ::parse(Parser& parser) {
    DiffZ diff;

    parser.match_bytes("HDIFF13&zstd\0", 13);
    diff.compressType = "zstd";

    diff.newDataSize = parser.read_varint();
    diff.oldDataSize = parser.read_varint();
    diff.coverCount = parser.read_varint();
    diff.coverBufSize = parser.read_varint();
    diff.compressedCoverBufSize = parser.read_varint();
    diff.rleCtrlBufSize = parser.read_varint();
    diff.compressedRleCtrlBufSize = parser.read_varint();
    diff.rleCodeBufSize = parser.read_varint();
    diff.compressedRleCodeBufSize = parser.read_varint();
    diff.newDataDiffSize = parser.read_varint();
    diff.compressedNewDataDiffSize = parser.read_varint();

    diff.coverBuf = CoverBuf::parse(parser, diff.coverBufSize.value, 
                                    diff.compressedCoverBufSize.value, 
                                    diff.coverCount.value);

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

HeadData HeadData::parse(Parser &parser, u64 size, u64 compressed_size,
                          u64 old_path_count, u64 new_path_count,
                          u64 old_ref_file_count, u64 new_ref_file_count) {
    HeadData head;

    std::vector<u8> data = parser.read_maybe_compressed(size, compressed_size);
    VectorIStream data_stream(data.data(), data.size());

    std::vector<std::string> oldFiles;
    std::vector<std::string> newFiles;

    std::vector<u8> oldFileOffsets;
    std::vector<u8> newFileOffsets;

    std::vector<VarInt> oldFileSizes;
    std::vector<VarInt> newFileSizes;

    // Checksums?
    // We only read them to be able to do the sanity check on the read data at the end
    std::vector<VarInt> unknown;

    Parser sub_parser = parser.sub_parser(data, "head_data");

    for(size_t i = 0; i < old_path_count; i++)
        oldFiles.push_back(sub_parser.read_until<char, std::string>('\0'));
    for(size_t i = 0; i < new_path_count; i++)
        newFiles.push_back(sub_parser.read_until<char, std::string>('\0'));

    for(size_t i = 0; i < old_ref_file_count; i++) {
        VarInt v = sub_parser.read_varint();
        // Still haven't found an offset larger than a byte, but we keep a sanity check
        assert(v.value < 128);
        oldFileOffsets.push_back(v.value);
    }
    for(size_t i = 0; i < new_ref_file_count; i++) {
        VarInt v = sub_parser.read_varint();
        assert(v.value < 128);
        newFileOffsets.push_back(v.value);
    }

    for(size_t i = 0; i < old_ref_file_count; i++)
        oldFileSizes.push_back(sub_parser.read_varint());
    for(size_t i = 0; i < new_ref_file_count; i++)
        newFileSizes.push_back(sub_parser.read_varint());

    for(size_t i = 0; i < new_ref_file_count; i++)
        unknown.push_back(sub_parser.read_varint());

    for(size_t i = 0, j = 0; i < old_path_count; i++) {
        if(oldFiles[i].ends_with('/') || oldFiles[i].empty()) {
            head.oldDirs.push_back(Directory(oldFiles[i]));
        }
        else {
            head.oldFiles.push_back(File(oldFiles[i], oldFileOffsets[j], 
                                         oldFileSizes[j].value));
            j++;
        }
    }

    for(size_t i = 0, j = 0; i < old_path_count; i++) {
        if(newFiles[i].ends_with('/') || newFiles[i].empty()) {
            head.newDirs.push_back(Directory(newFiles[i]));
        }
        else {
            head.newFiles.push_back(File(newFiles[i], newFileOffsets[j], 
                                         newFileSizes[j].value));
            j++;
        }
    }

    sub_parser.check_read(size);

    assert(head.oldFiles.size() == old_ref_file_count);
    assert(head.newFiles.size() == new_ref_file_count);
    assert(head.oldDirs.size() == old_path_count - old_ref_file_count);
    assert(head.newDirs.size() == new_path_count - new_ref_file_count);

    return head;
}

std::string HeadData::to_string() {
    std::ostringstream oss;
    oss << "HeadData {\n";

    for(int i = 0; i < 2; i++) {
        const auto& files = i ? newFiles : oldFiles;
        if(i)
            oss << "  newFiles: [\n";
        else 
            oss << "  oldFiles: [\n";

        for (const auto& file : files) {
            oss << "    {\n"
                << "      name: \"" << file.name << "\",\n"
                << "      fileOffset: " << static_cast<int>(file.fileOffset) << ",\n"
                << "      fileSize: " << file.fileSize << ",\n"
                << "    },\n";
        }
        oss << "  ],\n";
    }

    for(int i = 0; i < 2; i++) {
        const auto& dirs = i ? newDirs : oldDirs;
        if(i)
            oss << "  newDirs: [\n";
        else 
            oss << "  oldDirs: [\n";

        for (const auto& dir : dirs) {
            oss << "      name: \"" << dir.name << "\"\n";
        }
        oss << "  ]\n";
    }

    oss << "}";
    return oss.str();
}

DirDiff DirDiff::parse(Parser& parser) {
    DirDiff diff;

    // Hardcoded because they shouldn't change in foreseeable future
    // HDIFF19 & compressionType & checksumType \0 oldPathIsDir newPathIsDir
    parser.match_bytes("HDIFF19&zstd&fadler64\0\1\1", 24);
    diff.compressionType = "zstd";
    diff.checksumType = "fadler64";

    diff.oldPathIsDir = true;
    diff.newPathIsDir= true;

    diff.oldPathCount = parser.read_varint();
    diff.oldPathSumSize = parser.read_varint();
    diff.newPathCount = parser.read_varint();
    diff.newPathSumSize = parser.read_varint();
    diff.oldRefFileCount = parser.read_varint();
    diff.oldRefSize = parser.read_varint();
    diff.newRefFileCount = parser.read_varint();
    diff.newRefSize = parser.read_varint();

    // We discard all this since it seems to always be 0 in kuro diffs
    // We still check it's still really zero, just in case kuro changes the diffs
    // sameFilePairCount
    parser.match_varint(0);
    // sameFileSize
    parser.match_varint(0);
    // newExecuteCount
    parser.match_varint(0);
    // privateReservedDataSize
    parser.match_varint(0);
    // privateExternDataSize
    parser.match_varint(0);
    // externDataSize
    parser.match_varint(0);
    
    diff.headDataSize = parser.read_varint();
    diff.headDataCompressedSize = parser.read_varint();
    diff.checksumByteSize = parser.read_varint();

    diff.checksum = parser.read_bytes<u8>(diff.checksumByteSize.value * 4);
    diff.headData = HeadData::parse(parser, diff.headDataSize.value, diff.headDataCompressedSize.value, 
                                    diff.oldPathCount.value, diff.newPathCount.value,
                                    diff.oldRefFileCount.value, diff.newRefFileCount.value);
    diff.mainDiff = DiffZ::parse(parser);

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

void Parser::error(const std::string &err) {
    dwhbll::console::fatal("Parse error at {}: {}", format_context(), err);
    std::exit(1);
}

std::string Parser::format_context() const {
    return std::format("{}offset 0x{:X}", 
                      context_.empty() ? "" : context_ + " ", 
                      static_cast<size_t>(position()));
}

void Parser::check_read(u64 b) {
    if(position() != b) {
        error(std::format("Read size mismatch. Expected {}, got {}", b, 
                          static_cast<u64>(position())));
    }
}

std::vector<u8> Parser::read_maybe_compressed(u64 size, u64 compressed_size) {
    if(compressed_size > 0) {
        // Data is compressed
        std::vector<u8> compressed_data = read_bytes<u8>(compressed_size);
        u64 decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_size);
        if(decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
           decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
           decompressed_size != size) {
            error("Invalid compressed data: size mismatch");
        }

        std::vector<u8> data(decompressed_size);
        size_t result = ZSTD_decompress(data.data(), decompressed_size, compressed_data.data(),
                                        compressed_size);
        if(ZSTD_isError(result)) {
            error(std::format("Decompression failed: {}", ZSTD_getErrorName(result)));
        }

        return data;
    }
    else {
        // Data is not compressed
        return read_bytes<u8>(size);
    }
}

Parser Parser::sub_parser(const std::vector<u8>& data, const std::string& sub_context) {
    auto stream = std::make_unique<VectorIStream>(data.data(), data.size());
    return Parser(std::move(stream), format_context() + " -> " + sub_context);
}

VarInt Parser::read_varint(u8 kTagBit) {
   u8 byte = read<u8>();

   i64 res = byte & ((1 << (7 - kTagBit)) - 1);
   bool hasMore = (byte & (1 << (7 - kTagBit))) != 0;
   bool sign = kTagBit > 0 && byte & 0x80;

   if (hasMore) {
       do {
           byte = read<u8>();
           u64 nextBits = byte & 0x7F;
           res = (res << 7) | nextBits;
       } while ((byte & 0x80) != 0);
   }

   return { sign ? -res : res };
}

void Parser::match_varint(u64 expected) {
    VarInt v = read_varint();
    if(v.value != expected) {
        error(std::format("Expected VarInt {}, got {}", expected, v.value));
    }
}
