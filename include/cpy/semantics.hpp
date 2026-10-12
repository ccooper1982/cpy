#pragma once

#include <concepts>
#include <functional>

#include <ranges>

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/symbol_table.hpp>
#include <cpy/ast/ast_node.hpp>


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

  void process_block(Context& ctx, const NodeBlock& block);

  // functions
  void process_function_signatures(Context& ctx);
  void process_function_bodies(Context& ctx, const NodeBlock& block);

  // variables
  void process_variable_declaration(Context& ctx, const VariableDecl& decl);

  // TODO what's the parent param actually used for?
  // expressions
  VarType process_expression(Context& ctx, const AstNode& parent, const FunctionCall& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const VariableRef& expr);
  VarType process_expression(Context& ctx, const AstNode& parent, const BinaryExpression& expr);

  // operators
  VarType process_operator(Context& ctx, const VarType& a, const VarType& b, const BinaryExpression& expr);

  // conditions
  void process_condition(Context& ctx, const AstNode& parent, const ConditionIf& cond);

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
  void walk_expressions(const NodeBlock& block, Handler&& handler)
  {
    const constexpr ExpressionType et = ExprT::ExprType;

    walk_nodes<Expression>(block.nodes, [&](const Expression& expr_node)
    {
      if (expr_node.is_expr_type(et)) {
        handler(dynamic_cast<const ExprT&>(expr_node));
      }
    });
  }

  VarType handle_expression (Context& ctx, const AstNode& parent, const Expression& expr)
  {
    switch (expr.expr_type())
    {
      case ExpressionType::FuncCall:
        return process_expression(ctx, parent, dynamic_cast<const FunctionCall&>(expr));
      case ExpressionType::VariableRef:
        return process_expression(ctx, parent, dynamic_cast<const VariableRef&>(expr));
      case ExpressionType::Binary:
        return process_expression(ctx, parent, dynamic_cast<const BinaryExpression&>(expr));
      default:
        throw std::runtime_error{"process_block: unknown expression type"};
        break;
    }
  }

private:
  SymbolTable m_symbol_table;
};
