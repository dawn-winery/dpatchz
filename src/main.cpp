#include "../thirdparty/argparse.hpp"
#include "patching.hpp"
#include "src/parsing.hpp"
#include "dwhbll-logging.hpp"

u64 cache_size;

int main(int argc, char** argv) {
    argparse::ArgumentParser program("dpatchz");

    program.add_argument("diff_file");
    program.add_argument("source_dir");
    program.add_argument("output_dir")
        .help("Has to not exist or be empty");

    program.add_argument("-v", "--verbose")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("-c", "--cache")
        .help("Size in bytes of the read cache. Higher values should decrease time spent on I/O but increase memory usage. Default: 4096")
        .default_value(4096)
        .scan<'i', int>();

    program.add_argument("-i")
        .help("Inplace patching")
        .default_value(false)
        .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    bool verbose = program.get<bool>("verbose");
    if(verbose) {
        dwhbll::console::defaultLevel = dwhbll::console::Level::DEBUG;
    }

    std::filesystem::path diff_path = program.get<std::string>("diff_file");
    std::filesystem::path source_dir = program.get<std::string>("source_dir");
    std::filesystem::path output_dir = program.get<std::string>("output_dir");
    cache_size = program.get<int>("-c");
    bool inplace = program.get<bool>("-i");

    if(!std::filesystem::exists(diff_path) || std::filesystem::is_directory(diff_path)) {
        dwhbll::console::fatal("{} doesn't exist or is not a file", diff_path.string());
        return 1;
    }
    if(!std::filesystem::exists(source_dir) || !std::filesystem::is_directory(source_dir)) {
        dwhbll::console::fatal("{} doesn't exist or is not a directory", source_dir.string());
        return 1;
    }
    if(std::filesystem::exists(output_dir)) {
        if(!std::filesystem::is_directory(output_dir)) {
            dwhbll::console::fatal("{} exists and is not a directory", output_dir.string());
            return 1;
        }
        else if(!std::filesystem::is_empty(output_dir)) {
            dwhbll::console::fatal("{} exists and is not empty", output_dir.string());
            return 1;
        }
    }

    Parser parser(diff_path);
    DirDiff diff = DirDiff::parse(parser);

    dwhbll::console::debug("Parsed diff file:\n{}\n{}\n{}\n{}", diff.to_string(), 
                           diff.headData.to_string(), diff.mainDiff.to_string(),
                           diff.mainDiff.coverBuf.to_string());

    // Kuro diffs don't seem to be using RLE so we just ignore it
    // TODO: implement RLE anyway
    if(diff.mainDiff.compressedRleCodeBufSize.value > 0)
        parser.read_bytes<u8>(diff.mainDiff.compressedRleCodeBufSize.value);
    else
        parser.read_bytes<u8>(diff.mainDiff.rleCodeBufSize.value);

    if(diff.mainDiff.compressedRleCtrlBufSize.value > 0)
        parser.read_bytes<u8>(diff.mainDiff.compressedRleCtrlBufSize.value);
    else
        parser.read_bytes<u8>(diff.mainDiff.rleCtrlBufSize.value);

    diff.mainDiff.newDataOffset = parser.position();

    Patcher patcher(diff, diff_path);
    patcher.patch(source_dir, output_dir, inplace);
}
