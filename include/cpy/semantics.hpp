#pragma once

#include <concepts>
#include <functional>

#include <ranges>

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/symbol_table.hpp>
#include <cpy/ast/ast_node.hpp>


class Semantics
{
  struct Context
  {
    Context(SourceFile& src, Issues& issues) : ast(src), issues(issues)
    {}

    SourceFile& ast;
    Issues& issues;
  };

public:
  void process(Script& script);
  const SymbolTable& symbol_table() const { return m_symbol_table; }

private:
  template<typename NodeT>
  using NodeHandler = std::function<void(const NodeT&)>;

  // functions
  void process_function_defs(Context& ctx);
  void process_function_calls(Context& ctx);

  // variables
  void process_variable_declarations(Context& ctx);
  void process_variable_declaration(Context& ctx, const VariableDecl& decl);

  // expressions
  VarType process_expression(Context& ctx, const AstNode& parent, const FunctionCall& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const VariableRef& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const BinaryExpression& expr);

  // operands
  VarType process_operator(Context& ctx, const VarType& a, const VarType& b, const BinaryExpression& expr);

  // utils
  template<typename NodeT, typename Handler>
    requires std::derived_from<NodeT, AstNode> &&
             std::convertible_to<decltype(NodeT::Type), NodeType>
  void walk_nodes(const std::vector<std::unique_ptr<AstNode>>& nodes, Handler&& handler)
  {
    const constexpr NodeType nt = NodeT::Type;

    auto by_node_type = [](const std::unique_ptr<AstNode>& n) {
      return n->is_node_type(nt);
    };

    for (const auto& node : nodes | vw::filter(by_node_type)) {
      handler(dynamic_cast<const NodeT&>(*node));
    }
  }

  template<typename ExprT, typename Handler>
    requires std::derived_from<ExprT, Expression> &&
             std::convertible_to<decltype(ExprT::ExprType), ExpressionType>
  void walk_expressions(const SourceFile& root, Handler&& handler)
  {
    const constexpr ExpressionType et = ExprT::ExprType;

    walk_nodes<Expression>(root.nodes, [&](const Expression& expr_node)
    {
      if (expr_node.is_expr_type(et)) {
        handler(dynamic_cast<const ExprT&>(expr_node));
      }
    });
  }

private:
  SymbolTable m_symbol_table;
};
