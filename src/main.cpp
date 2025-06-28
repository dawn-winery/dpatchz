#include "../thirdparty/argparse.hpp"

int main(int argc, char** argv) {
    argparse::ArgumentParser program("dpatchz");

    program.add_argument("diff_file");
    program.add_argument("source_dir");
    program.add_argument("output_dir")
        .help("Has to not exist or be empty")
        .nargs(1)
        .default_value("");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::filesystem::path diff_path = program.get<std::string>("diff_file");
    std::filesystem::path source_dir = program.get<std::string>("source_dir");
    std::filesystem::path output_dir = program.get<std::string>("output_dir");

    
}
