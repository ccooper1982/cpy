#include "cpy/ast/expressions.hpp"
#include "cpy/common.hpp"
#include "cpy/exec/operators.hpp"
#include <cpy/exec/executor.hpp>
#include <stdexcept>
#include <utility>


static VarValue value_from_literal_expr(const Expression& expr)
{
  switch (expr.expr_type())
  {
    using enum ExpressionType;

    case Int:
      return static_cast<const IntegerLiteral&>(expr).value();
    case Bool:
      return static_cast<const BooleanLiteral&>(expr).value();
    case Dec:
      return static_cast<const DecimalLiteral&>(expr).value();
    case String:
      return static_cast<const StringLiteral&>(expr).value();
    default:
      throw std::runtime_error{"Expression type is not literal"};
  }
}


[[nodiscard]] static bool is_true(const VarValue& value)
{
    const auto * b = std::get_if<CpyBool>(&value);
    return b != nullptr && *b;
}


Executor::Executor(const SymbolTable& sym_table)
  : m_sym_table(sym_table)
{

}

VarValue Executor::execute(const NodeBlock& block, std::map<VarName, Variable>&& func_args)
{
  auto is_executable = [](const std::unique_ptr<AstNode>& node)
  {
    return  node->node_type() == NodeType::VariableDecl ||
            node->node_type() == NodeType::VariableAssign ||
            node->node_type() == NodeType::ConditionIf ||
            node->node_type() == NodeType::Expression;
  };

  VarValue result;

  push_stack(block, std::move(func_args));

  try
  {
    // this ignores NodeType::NodeBlock because we a block can only appear in a function
    // or condition body
    for(const auto& node : block.nodes | vw::filter(is_executable))
    {
      switch (node->node_type())
      {
        case NodeType::VariableDecl:
          execute(block, static_cast<const VariableDecl&>(*node));
          break;
        case NodeType::VariableAssign:
          execute(block, static_cast<const VariableAssign&>(*node));
          break;
        case NodeType::Expression:
          result = execute(block, static_cast<const Expression&>(*node));
          break;
        case NodeType::ConditionIf:
          result = execute(block, static_cast<const ConditionIf&>(*node));
          break;
        default:
          throw std::runtime_error{"Unexpected node type in block"};
      }
    }
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
  }

  pop_stack();

  return result;
}


VarValue Executor::execute (const NodeBlock& block, const ConditionIf& cond)
{
  VarValue result;

  auto cond_result = execute(block, *cond.if_.condition);

  if (is_true(cond_result)) {
    result = execute(cond.if_body());
  }
  else
  {
    for(const auto& [cond, body] : cond.else_if)
    {
      if (cond_result = execute(block, *cond); is_true(cond_result))
      {
        result = execute(body);
        break;
      }
    }

    if (!is_true(cond_result) && cond.have_else()) {
      result = execute(*cond.else_body);
    }
  }

  return result;
}


void Executor::execute (const NodeBlock& block, const VariableDecl& decl)
{
  auto& var = get_variable(decl.var_name);

  if (decl.is_literal_initialised()) {
    var.value() = value_from_literal_expr(*decl.initialiser);
  }
  else if (decl.has_initialiser()) {
    var.value() = execute(block, *decl.initialiser);
  }
}


void Executor::execute (const NodeBlock& block, const VariableAssign& assign)
{
  auto& var = get_variable(assign.name);

  if (assign.expr->is_literal()) {
    var.value() = value_from_literal_expr(*assign.expr);
  }
  else {
    var.value() = execute(block, *assign.expr);
  }
}


VarValue Executor::execute (const NodeBlock& block, const Expression& expr)
{
  VarValue result;

  if (expr.is_literal()) {
    result = value_from_literal_expr(expr);
  }
  else if (expr.is_expr_type(ExpressionType::FuncCall))
  {
    const auto& call = static_cast<const FunctionCall&>(expr);
    const auto& def = m_sym_table.get_function(call.name);

    std::map<VarName, Variable> args;

    for (size_t i = 0 ; i < call.args.size() ; ++i)
    {
      auto val = execute(block, *call.args[i].expr);

      Variable variable;

      if (def.params[i].is_ref)
      {
        // a: int;
        // foo (ref a);
        // we need to find variable a
        const auto& var_ref = static_cast<const VariableUse&>(*call.args[i].expr);
        variable.set_ref_target(&get_variable(var_ref.name));
      }
      else {
        variable.value() = val;
      }

      args.emplace(def.params[i].name, std::move(variable));
    }

    result = execute(def.ast->body, std::move(args));
  }
  else if (expr.is_expr_type(ExpressionType::Binary))
  {
    const auto& bin_expr = static_cast<const BinaryExpression&>(expr);
    result = apply_operator(block, *bin_expr.lhs, *bin_expr.rhs, bin_expr.op);
  }
  else if (expr.is_expr_type(ExpressionType::Return))
  {
    if (const auto& ret = static_cast<const Return&>(expr); ret.has_expression())
    {
      if (ret.return_expr->is_literal()) {
        result = value_from_literal_expr(*ret.return_expr);
      }
      else {
        result = execute(block, *ret.return_expr);
      }
    }
  }
  else if (expr.is_expr_type(ExpressionType::VariableUse))
  {
    const auto& ref = static_cast<const VariableUse&>(expr);
    const auto& variable = get_variable(ref.name);
    result = variable.value();
  }

  return result;
}


void Executor::push_stack (const NodeBlock& block, std::map<VarName, Variable> func_args)
{
  const auto parent = m_stack.empty() ? nullptr : m_stack.top().get();

  if (const auto block_vars = m_sym_table.get_variables(block); block_vars) {
    m_stack.emplace(std::make_unique<StackEntry>(*block_vars, parent));
  }

  for (auto& [name, var] : func_args)
  {
    if (var.is_ref()) {
      m_stack.top()->get_var(name).set_ref_target(var.get_ref());
    }
    else {
      m_stack.top()->get_var(name).value() = std::move(var.value());
    }
  }
}


void Executor::pop_stack ()
{
  if (!m_stack.empty()) {
    m_stack.pop();
  }
  else {
    // this shouldn't happen, but if so, we're not managing the stack correctly
    throw std::logic_error{"Executor stack called on empty stack"};
  }
}


Variable& Executor::get_variable (const VarName name)
{
  StackEntry * entry = m_stack.empty() ? nullptr : m_stack.top().get();

  while (entry && !entry->have_var(name)) {
    entry = entry->get_parent();
  }

  if (!entry) {
    throw std::runtime_error{std::format("get_variable(): failed to find resolved variable: {}", name)};
  }

  auto& var = entry->get_var(name);
  return var.is_ref() ? var.get_ref_target() : var;
}


VarValue Executor::apply_operator(const NodeBlock& block, const Expression& lhs, const Expression& rhs, const BinaryOperator op)
{
  VarValue lhs_val, rhs_val;

  if (lhs.is_literal()) {
    lhs_val = value_from_literal_expr(lhs);
  }
  else {
    lhs_val = execute(block, lhs);
  }

  if (rhs.is_literal()) {
    rhs_val = value_from_literal_expr(rhs);
  }
  else {
    rhs_val = execute(block, rhs);
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
}
