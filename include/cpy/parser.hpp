#pragma once
#include <string_view>

#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>

#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_cpy();

class Parser
{
public:
  Parser() = default;
  ~Parser();

  Script parse(const std::string_view src);
  Script parse(const fs::path src_file);

private:
  void parse(Script& script);

private:
  TSParser * m_parser{};
  TSTree * m_tree{};
};
