project(
  'cpy',
  ['c','cpp'],
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
  ],
)

tree_sitter = dependency('tree-sitter')
gtest = dependency('gtest')
gtest_main = dependency('gtest_main')

cpy_sources = files(
  'src/main.cpp',
  'src/parser.cpp',
  'grammar/src/parser.c',
)

include_dirs = include_directories('grammar/src','include')

executable('cpy', cpy_sources,
  include_directories: include_dirs,
  dependencies: [tree_sitter],
  install: false
)

cpy_tests = executable(
  'cpy_tests',
  files('tests/ast.cpp', 'src/parser.cpp', 'grammar/src/parser.c'),
  include_directories: include_dirs,
  dependencies: [tree_sitter, gtest, gtest_main],
)

test('cpy tests', cpy_tests)
