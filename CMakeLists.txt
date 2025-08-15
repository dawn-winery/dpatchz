cmake_minimum_required(VERSION 3.31)
project(dpatchz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

set(DPATCHZ_SOURCES
    src/main.cpp
    src/parsing.cpp
    src/patching.cpp
    src/dwhbll-logging.cpp
)

pkg_check_modules(libzstd REQUIRED libzstd)

add_executable(dpatchz ${DPATCHZ_SOURCES})
target_include_directories(dpatchz PRIVATE src)
target_link_libraries(dpatchz PRIVATE zstd)
