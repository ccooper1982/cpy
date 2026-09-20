#pragma once
#include <string_view>
#include <tree_sitter/api.h>
#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>

extern "C" const TSLanguage *tree_sitter_cpy();

struct Script
{
  std::string_view src;
  std::unique_ptr<SourceFile> ast{};
  Issues issues{};
};

class Parser
{
public:
  ~Parser();

  Script parse(const std::string_view src);

private:
  TSParser * m_parser{};
  TSTree * m_tree{};
};
