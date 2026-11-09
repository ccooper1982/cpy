#include "cpy/ast/ast.hpp"
#include "cpy/ast/expressions.hpp"
#include "cpy/common.hpp"
#include "cpy/exec/operators.hpp"
#include <algorithm>
#include <cpy/exec/executor.hpp>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>


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

[[nodiscard]] static bool is_true(const ExecutionResult& result)
{
  return is_true(result.value);
}


Executor::Executor(const SymbolTable& sym_table, const RuntimeContext& rt_context)
  : m_sym_table(sym_table)
  , m_rt_context(rt_context)
{

}


ExecutionResult Executor::execute(const NodeBlock& block, std::map<VarName, Variable>&& func_args)
{
  ExecutionResult result;

  push_stack(block, std::move(func_args));

  for(const auto& node : block.nodes() | vw::filter(is_node_executable))
  {
    switch (node->node_type())
    {
      case NodeType::Expression:
        result = execute(block, static_cast<const Expression&>(*node));
        break;
      case NodeType::ConditionIf:
        result = execute(block, static_cast<const ConditionIf&>(*node));
        break;
      case NodeType::LoopWhile:
        result = execute(block, static_cast<const LoopWhile&>(*node));
        break;
      case NodeType::VariableDecl:
        execute(block, static_cast<const VariableDecl&>(*node));
        break;
      case NodeType::VariableAssign:
        execute(block, static_cast<const VariableAssign&>(*node));
        break;
      default:
        throw std::runtime_error{"Unexpected node type in block"};
    }

    if (result.returned) {
      break;
    }
  }

  pop_stack();

  return result;
}


ExecutionResult Executor::execute (const NodeBlock& block, const LoopWhile& loop)
{
  // execute the loop's condition expression on each iteration
  ExecutionResult result;

  while (is_true(execute(block, *loop.expr)))
  {
    if (result = execute(loop.body); result.returned) {
      break;
    }
  }

  return result;
}


ExecutionResult Executor::execute (const NodeBlock& block, const ConditionIf& cond)
{
  ExecutionResult result;

  auto if_result = execute(block, *cond.if_.condition);

  if (is_true(if_result)) {
    result = execute(cond.if_body());
  }
  else
  {
    auto condition_true = [this, &block](const ConditionIf::Condition& cb)
    {
      return is_true(execute(block, *cb.condition));
    };

    const auto it = rg::find_if(cond.else_if, condition_true);

    if (it != rg::end(cond.else_if)) {
      result = execute(it->body);
    }
    else if (cond.have_else()) {
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
    var.value() = execute(block, *decl.initialiser).value;
  }
}


void Executor::execute (const NodeBlock& block, const VariableAssign& assign)
{
  auto& var = get_variable(assign.name);

  if (assign.expr->is_literal()) {
    var.value() = value_from_literal_expr(*assign.expr);
  }
  else {
    var.value() = execute(block, *assign.expr).value;
  }
}


ExecutionResult Executor::execute (const NodeBlock& block, const Expression& expr)
{
  ExecutionResult result;

  if (expr.is_literal()) {
    result = value_from_literal_expr(expr);
  }
  else if (expr.is_expr_type(ExpressionType::FuncCall)) {
    result = execute(block, static_cast<const FunctionCall&>(expr));
  }
  else if (expr.is_expr_type(ExpressionType::Unary))
  {
    const auto& unary_expr = static_cast<const UnaryExpression&>(expr);
    result = apply_operator(block, *unary_expr.operand, unary_expr.op);

    if (unary_expr.op == UnaryOperator::PreDecrement || unary_expr.op == UnaryOperator::PreIncrement)
    {
      // semantic checker should only allow `++a`, but for sanity we confirm
      if (unary_expr.operand->expr_type() == ExpressionType::VariableUse)
      {
        const auto& var_name = static_cast<const VariableUse&>(*unary_expr.operand).name;
        auto& var = get_variable(var_name);
        var = result.value;
      }
    }
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
    result.returned = true;
  }
  else if (expr.is_expr_type(ExpressionType::VariableUse))
  {
    const auto& ref = static_cast<const VariableUse&>(expr);
    const auto& variable = get_variable(ref.name);
    result = variable.value();
  }

  return result;
}


ExecutionResult Executor::execute (const NodeBlock& block, const FunctionCall& call)
{
  const auto& def = m_sym_table.get_function(call.name);

  if (m_sym_table.has_core_function(call.name))
  {
    std::vector<VarValue> values;

    for (size_t i = 0 ; i < call.args.size() ; ++i)
    {
      auto frame = execute(block, *call.args[i].expr);
      values.push_back(std::move(frame.value));
    }

    return CoreFunctions.at(call.name).implementation(m_rt_context, values);
  }
  else
  {
    std::map<VarName, Variable> args;

    for (size_t i = 0 ; i < call.args.size() ; ++i)
    {
      Variable variable;

      if (def.params[i].is_ref)
      {
        //  a: int;
        //  foo (ref a);
        // we need to find `a`
        const auto& var_ref = static_cast<const VariableUse&>(*call.args[i].expr);
        variable.set_ref_target(&get_variable(var_ref.name));
      }
      else {
        variable.value() = execute(block, *call.args[i].expr).value;
      }

      args.emplace(def.params[i].name, std::move(variable));
    }

    return execute(def.ast->body, std::move(args));
  }
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


VarValue Executor::apply_operator(const NodeBlock& block, const Expression& operand, const UnaryOperator op)
{
  ExecutionResult result = execute(block, operand);

  switch (op)
  {
    using enum UnaryOperator;

    case Not:
      return !result.value;
    case Negate:
      return -result.value;
    case BitwiseNot:
      return ~result.value;
    case PreDecrement:
      return result.value - 1;
    case PreIncrement:
      return result.value + 1;
  }
}


VarValue Executor::apply_operator(const NodeBlock& block, const Expression& lhs, const Expression& rhs, const BinaryOperator op)
{
  const auto& lhs_val = execute(block, lhs).value;
  const auto& rhs_val = execute(block, rhs).value;

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
    case LogicalAnd:
      return lhs_val && rhs_val;
    case LogicalOr:
      return lhs_val || rhs_val;
  }
}
