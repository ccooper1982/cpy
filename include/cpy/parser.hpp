#pragma once
#include <memory>
#include <string_view>

#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>

#include <tree_sitter/api.h>


class Parser
{
  struct TSParserDeleter
  {
    void operator()(TSParser* parser) const
    {
      if (parser) {
       ts_parser_delete(parser);
      }
    }
  };

  struct TSTreeDeleter
  {
    void operator()(TSTree* tree) const
    {
      if (tree) {
        ts_tree_delete(tree);
      }
    }
  };

  using TSParserPtr = std::unique_ptr<TSParser, TSParserDeleter>;
  using TSTreePtr = std::unique_ptr<TSTree, TSTreeDeleter>;

public:
  Script parse(const std::string_view src);
  Script parse(const fs::path& src_file);

private:
  void parse();

  void parse_script(const TSNode& ts_root);

  std::unique_ptr<Expression> parse_expression(const TSNode& expr_node);
  std::unique_ptr<VariableDecl> parse_variable_decl(const TSNode& ts_node);

  std::unique_ptr<AstNode> parse_statement (const TSNode& ts_statement);
  std::vector<std::unique_ptr<AstNode>> parse_statements (const TSNode& ts_statements);

  std::vector<std::unique_ptr<Expression>> parse_function_call_args(const TSNode& args_node);
  std::unique_ptr<FunctionCall> parse_function_call(const TSNode& func_call);
  std::unique_ptr<FunctionDef> parse_function_def(const TSNode& ts_node);

  std::string_view from_source (const TSNode& node);

private:
  TSParserPtr m_parser;
  TSTreePtr m_tree;

  std::unique_ptr<SourceFile> m_ast;
  std::unique_ptr<std::string> m_src;
  std::unique_ptr<Issues> m_issues;
};
