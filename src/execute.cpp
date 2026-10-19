#include <cpy/exec/executor.hpp>
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

  std::unreachable();
}


Executor::Executor(const SymbolTable& sym_table)
  : m_sym_table(sym_table)
{

}

VarValue Executor::execute(const NodeBlock& block, const std::map<VarName, VarValue>&& func_args)
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

        /*
        case NodeType::ConditionIf:
          process_condition(ctx, *node, block, static_cast<const ConditionIf&>(*node));
          break;
        case NodeType::NodeBlock:
          process_block(ctx, static_cast<const NodeBlock&>(*node));
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

  return result;
}


void Executor::execute (const NodeBlock& block, const VariableDecl& decl)
{
  auto& var = get_variable(decl.var_name);

  if (decl.is_literal_initialised()) {
    var.value = value_from_literal_expr(*decl.initialiser);
  }
  else if (decl.has_initialiser()) {
    var.value = execute(block, *decl.initialiser);
  }
}

void Executor::execute (const NodeBlock& block, const VariableAssign& assign)
{
  auto& var = get_variable(assign.name);

  if (assign.expr->is_literal()) {
    var.value = value_from_literal_expr(*assign.expr);
  }
  else {
    var.value = execute(block, *assign.expr);
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

    std::map<VarName, VarValue> args;

    for (size_t i = 0 ; i < call.args.size() ; ++i)
    {
      auto val = execute(block, *call.args[i]);
      args.emplace(def.params[i].name, std::move(val));
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
  else if (expr.is_expr_type(ExpressionType::VariableRef))
  {
    const auto& ref = static_cast<const VariableRef&>(expr);
    const auto& variable = get_variable(ref.name);
    result = variable.value;
  }

  return result;
}


void Executor::push_stack (const NodeBlock& block, std::map<VarName, VarValue> vals)
{
  const auto parent = m_stack.empty() ? nullptr : m_stack.top().get();

  if (const auto vars = m_sym_table.get_variables(block); vars) {
    m_stack.emplace(std::make_unique<StackEntry>(*vars, parent));
  }

  for (const auto& [name, val] : vals) {
    m_stack.top()->get_var(name).value = std::move(val);
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

  return entry->get_var(name);
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
