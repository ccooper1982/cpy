#pragma once
#include <memory>
#include <string_view>

#include <cpy/ast/ast.hpp>

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

  NodePtr<Expression> parse_expression(const TSNode& expr_node);

  NodePtr<VariableDecl> parse_variable_decl(const TSNode& ts_node);
  NodePtr<VariableAssign> parse_variable_assign(const TSNode& ts_node);

  NodePtr<AstNode> parse_statement (const TSNode& ts_statement);
  std::vector<NodePtr<AstNode>> parse_statements (const TSNode& ts_statements);

  std::vector<FunctionArgument> parse_function_call_args(const TSNode& args_node);
  NodePtr<FunctionCall> parse_function_call(const TSNode& func_call);
  NodePtr<FunctionDef> parse_function_def(const TSNode& ts_node);

  NodePtr<ConditionIf> parse_condition_if(const TSNode& ts_node);

  NodePtr<Return> parse_return(const TSNode& ts_node);

  std::string_view from_source (const TSNode& node);

private:
  TSParserPtr m_parser;
  TSTreePtr m_tree;

  NodePtr<SourceFile> m_ast;
  std::unique_ptr<std::string> m_src;
  std::unique_ptr<Issues> m_issues;
};
