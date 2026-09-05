project(
  'cpy',
  ['c','cpp'],
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
  ],
)

# cpp = meson.get_compiler('cpp')

# Dependencies
tree_sitter = dependency('tree-sitter')

cpy_sources = files(
  'src/main.cpp',
  'grammar/src/parser.c',
)

include_dirs = include_directories('grammar/src','include')

executable('cpy', cpy_sources,
  include_directories: include_dirs,
  dependencies: [tree_sitter], install: false
)

# Tests
# test(
#   'cpy --version',
#   cpy,
#   args: ['--version'],
# )
