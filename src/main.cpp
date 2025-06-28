#include "../thirdparty/argparse.hpp"
#include "src/parsing.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fstream>

int main(int argc, char** argv) {
    argparse::ArgumentParser program("dpatchz");

    program.add_argument("diff_file");
    program.add_argument("source_dir");
    program.add_argument("output_dir")
        .help("Has to not exist or be empty");

    program.add_argument("-v", "--verbose")
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

    std::ifstream file(diff_path, std::ios::binary);
    DirDiff diff = DirDiff::parse(file);
    file.close();

    dwhbll::console::debug("Parsed diff file:\n{}\n{}\n{}", diff.to_string(), 
                           diff.headData.to_string(), diff.mainDiff.to_string());
}
