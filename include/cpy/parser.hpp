#pragma once
#include <string_view>

#include <tree_sitter/api.h>
#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>

extern "C" const TSLanguage *tree_sitter_cpy();

struct Script
{
  std::string src{};
  fs::path file{};
  std::unique_ptr<SourceFile> ast{};
  Issues issues{};
};


class Parser
{
public:
  Parser() = default;
  ~Parser();

  void parse(const std::string_view src);
  bool parse(const fs::path src_file);

  const std::unique_ptr<SourceFile>& ast() const { return m_script.ast; }
  const Issues& issues() const { return m_script.issues; }
  const std::string& src() const { return m_script.src; }

private:
  void parse(Script& script);

private:
  TSParser * m_parser{};
  TSTree * m_tree{};
  Script m_script;
};
