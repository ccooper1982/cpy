#pragma once

#include <cpy/ast/ast_node.hpp>
#include <cpy/ast/expressions.hpp>
#include <cpy/ast/ast.hpp>
#include <cpy/exec/operators.hpp>
#include <cpy/common.hpp>
#include <cpy/symbol_table.hpp>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>


using VarName = std::string_view;


static VarValue value_from_type (const VarType type)
{
  if (!type.is_type<BuiltInType>()) {
    throw std::runtime_error{"Unsupported VarType"};
  }

  switch (*type.value_as<BuiltInType>())
  {
    using enum BuiltInType;

    case Int:
      return int64_t{};
      break;

    case Decimal:
      return double{};
      break;

    case Bool:
      return bool{};
      break;

    case String:
      return std::string{};
      break;

    default:
      throw std::runtime_error{"BuiltInType is invalid"};
      break;
  }
}


static VarValue value_from_literal_expr(const Expression& expr)
{
  switch (expr.expr_type())
  {
    using enum ExpressionType;

    case Int:
      return dynamic_cast<const IntegerLiteral&>(expr).value();
    case Bool:
      return dynamic_cast<const BooleanLiteral&>(expr).value();
    case Dec:
      return dynamic_cast<const DecimalLiteral&>(expr).value();
    case String:
      return dynamic_cast<const StringLiteral&>(expr).value();
    default:
      throw std::runtime_error{"Expression type is not literal"};
  }

  std::unreachable();
}



struct Variable
{
  Variable(const VarValue& v) : value(v)
  {}

  VarValue value;
};

class StackEntry
{
public:
  StackEntry(VariableMap var_map, StackEntry * parent)
    : m_var_syms(var_map)
    , m_parent(parent)
  {
    init();
  }

  Variable& get_var(const VarName name)
  {
    return m_vars.at(name);
  }

  bool have_var (const VarName name) const
  {
    return m_vars.contains(name);
  }

  bool has_parent() const { return m_parent != nullptr; }
  StackEntry * get_parent() const { return m_parent; }

private:
  void init()
  {
    for (const auto& [var, type] : m_var_syms)
    {
      if (!type.type.is_type<BuiltInType>()) {
        throw std::runtime_error{"Only BuiltInType is supported"};
      }
      else {
        m_vars.emplace(var, value_from_type(type.type));
      }
    }
  }

private:
  VariableMap m_var_syms;
  std::map<VarName, Variable> m_vars;
  StackEntry * m_parent{};
};


class Executor
{
public:
  Executor(const SymbolTable& sym_table, const SourceFile& ast)
    : m_sym_table(sym_table)
    , m_ast(ast)
  {}

  void execute(const NodeBlock& block)
  {
    auto is_executable = [](const std::unique_ptr<AstNode>& node)
    {
      return  node->node_type() == NodeType::VariableDecl ||
              node->node_type() == NodeType::VariableAssign ||
              node->node_type() == NodeType::ConditionIf ||
              node->node_type() == NodeType::Expression;
    };

    push_stack(block);

    try
    {
      for(const auto& node : block.nodes | vw::filter(is_executable))
      {
        switch (node->node_type())
        {
          case NodeType::VariableDecl:
            execute(dynamic_cast<const VariableDecl&>(*node));
            break;
          case NodeType::VariableAssign:
            execute(dynamic_cast<const VariableAssign&>(*node));
            break;
          case NodeType::Expression:
            execute(dynamic_cast<const Expression&>(*node));
            break;

          /*
          case NodeType::ConditionIf:
            process_condition(ctx, *node, block, dynamic_cast<const ConditionIf&>(*node));
            break;
          case NodeType::NodeBlock:
            process_block(ctx, dynamic_cast<const NodeBlock&>(*node));
            break;
          */

          default:
            //std::unreachable();
            std::cerr << "TODO\n";
            break;
        }
      }
    }
    catch (const std::exception& ex)
    {
      std::cerr << ex.what();
    }

    pop_stack();
  }

  void execute (const VariableDecl& decl)
  {
    auto& var = get_variable(decl.var_name);

    if (decl.is_literal_initialised()) {
      var.value = value_from_literal_expr(*decl.initialiser);
    }
  }

  void execute (const VariableAssign& assign)
  {
    auto& var = get_variable(assign.name);

    if (assign.expr->is_literal()) {
      var.value = value_from_literal_expr(*assign.expr);
    }
    else {
      var.value = execute(*assign.expr);
    }
  }

  VarValue execute (const Expression& expr)
  {
    VarValue result;

    if (expr.is_expr_type(ExpressionType::FuncCall))
    {
      const auto& call = dynamic_cast<const FunctionCall&>(expr);

      const auto& def = m_sym_table.get_function(call.name);
      execute(def.ast->body);
    }
    else if (expr.is_expr_type(ExpressionType::Binary))
    {
      const auto& bin_expr = dynamic_cast<const BinaryExpression&>(expr);
      const auto& lhs = *bin_expr.lhs;
      const auto& rhs = *bin_expr.rhs;

      result = apply_operator(lhs, rhs, bin_expr.op);
    }

    return result;
  }

private:

  // variable stack //
  void push_stack (const NodeBlock& block)
  {
    const auto parent = m_stack.empty() ? nullptr : m_stack.top().get();

    if (const auto vars = m_sym_table.get_variables(block); vars) {
      m_stack.emplace(std::make_unique<StackEntry>(*vars, parent));
    }
  }

  void pop_stack ()
  {
    if (!m_stack.empty()) {
      m_stack.pop();
    }
  }

  Variable& get_variable (const VarName name)
  {
    StackEntry * entry = m_stack.empty() ? nullptr : m_stack.top().get();

    while (entry)
    {
      if (!entry->have_var(name))
        entry = entry->get_parent();
      else
        break;
    }

    if (!entry) {
      throw std::runtime_error{std::format("get_variable(): failed to find resolved variable: {}", name)};
    }

    return entry->get_var(name);
  }


  // operators //
  VarValue apply_operator(const Expression& lhs, const Expression& rhs, const BinaryOperator op)
  {
    VarValue lhs_val;
    VarValue rhs_val;

    if (lhs.is_literal()) {
      lhs_val = value_from_literal_expr(lhs);
    }


    if (rhs.is_literal()) {
      rhs_val = value_from_literal_expr(rhs);
    }

    switch (op)
    {
      using enum BinaryOperator;

      case Add:
        return lhs_val + rhs_val;
      case Subtract:
        return lhs_val - rhs_val;
      case Divide:
        return lhs_val / rhs_val;
      case Multiply:
        return lhs_val * rhs_val;
      case Less:
        return lhs_val < rhs_val;
      case LessEqual:
        return lhs_val <= rhs_val;
      case Greater:
        return lhs_val > rhs_val;
      case GreaterEqual:
        return lhs_val >= rhs_val;
      case Equal:
        return lhs_val == rhs_val;
      case NotEqual:
        return lhs_val != rhs_val;
    }

    std::unreachable();
  }

private:
  const SymbolTable& m_sym_table;
  const SourceFile& m_ast;
  std::stack<std::unique_ptr<StackEntry>> m_stack;
};
