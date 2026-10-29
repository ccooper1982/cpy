#pragma once

#include "cpy/ast/ast_node.hpp"
#include "cpy/ast/expressions.hpp"
#include <concepts>
#include <functional>
#include <ranges>

#include <cpy/ast/ast.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/symbol_table.hpp>


///
/// 1. Function definitions
///   - function name does not exist
///   - parameters are valid types
///   - return type is valid
/// 2. Root block:
///   - Function calls: function exists, parameters correct
///   - Var declaration: don't already exist, type and initialiser valid
///   - Conditions: condition is returns boolean, validate body block
/// 3. Function bodies:
///   - Enter each defined function, validating body
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

  void process_block(Context& ctx, const AstNode& parent, const NodeBlock& block, const bool add_scope = true);

  // functions
  void process_function_signatures(Context& ctx);
  void process_function_bodies(Context& ctx, const NodeBlock& block);

  // variables
  void process_variable_declaration(Context& ctx, const NodeBlock& block, const VariableDecl& decl);
  void process_variable_assign(Context& ctx, const NodeBlock& block, const VariableAssign& assign);

  // TODO what's the parent param actually used for?
  // expressions
  VarType process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const FunctionCall& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const VariableUse& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const BinaryExpression& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const Return& expr);

  // operators
  VarType process_operator(Context& ctx, const VarType& a, const VarType& b, const BinaryExpression& expr);

  // conditions
  void process_condition(Context& ctx, const AstNode& parent, const NodeBlock& block, const ConditionIf& cond);

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
      handler(static_cast<const NodeT&>(*node));
    }
  }

  template<typename ExprT, typename Handler>
    requires std::derived_from<ExprT, Expression> &&
             std::convertible_to<decltype(ExprT::ExprType), ExpressionType>
  void walk_expressions(const NodeBlock& block, Handler&& handler)
  {
    const constexpr ExpressionType et = ExprT::ExprType;

    walk_nodes<Expression>(block.nodes, [&](const Expression& expr_node)
    {
      if (expr_node.is_expr_type(et)) {
        handler(static_cast<const ExprT&>(expr_node));
      }
    });
  }

  template<typename ExprT>
    requires std::derived_from<ExprT, Expression> &&
             std::convertible_to<decltype(ExprT::ExprType), ExpressionType>
  size_t expression_count (const NodeBlock& block)
  {
    size_t count{};
    walk_expressions<ExprT>(block, [&]([[maybe_unused]] const ExprT& expr){ ++count; });

    // TODO walk_nodes() cannot accept multiple node types
    for (const auto& node : block.nodes)
    {
      if (node->is_node_type(NodeType::NodeBlock))
        count += expression_count<ExprT>(static_cast<const NodeBlock&> (*node));
      else if (node->is_node_type(NodeType::ConditionIf))
      {
        const auto& cond = static_cast<const ConditionIf&> (*node);

        count += expression_count<ExprT>(cond.if_body());

        for (const auto& elif : cond.else_if) {
          count += expression_count<ExprT>(elif.body);
        }

        if (cond.have_else()) {
          count += expression_count<ExprT>(*cond.else_body);
        }
      }
    }

    return count;
  }

  VarType handle_expression (Context& ctx, const AstNode& parent, const NodeBlock& block, const Expression& expr)
  {
    switch (expr.expr_type())
    {
      case ExpressionType::FuncCall:
      {
        // const auto& func_call = static_cast<const FunctionCall&>(expr);
        // const auto func_def = m_symbol_table.get_function(func_call.name).ast;
        return process_expression(ctx, parent, block, static_cast<const FunctionCall&>(expr));
      }
      case ExpressionType::VariableUse:
        return process_expression(ctx, parent, block, static_cast<const VariableUse&>(expr));
      case ExpressionType::Return:
        return process_expression(ctx, parent, block, static_cast<const Return&>(expr));
      case ExpressionType::Binary:
        return process_expression(ctx, parent, block, static_cast<const BinaryExpression&>(expr));
      default:
        return expr.get_var_type(); // literal
    }
  }

private:
  SymbolTable m_symbol_table;
};
