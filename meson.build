project(
  'dpatchz',
  'cpp',
  license: 'GPL-3.0',
  default_options: ['cpp_std=c++23'],
)

source_files = [
  'src/main.cpp',
  'src/logging.cpp',
  'src/parsing.cpp'
]

zstd = dependency('libzstd')

executable(
  meson.project_name(),
  source_files,
  include_directories: include_directories(['thirdparty', 'src']),
  dependencies: [zstd]
)
