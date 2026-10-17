#include "cpy/ast/ast_node.hpp"
#include <cpy/ast/ast.hpp>
#include <cpy/issues.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


// Semantic checker
//  │
//  ├── type resolution
//  ├── symbol/function lookup
//  ├── type checking
//  └── diagnostics

void Semantics::process(Script& script)
{
  if (!(script.ast && script.issues)) {
    throw std::runtime_error{"Ast and/or Issues not allocated"};
  }

  Context ctx(*script.ast, *script.issues);

  process_function_signatures(ctx);

  m_symbol_table.enter_scope(&ctx.ast.block);

  process_block(ctx, ctx.ast.block, false);
  process_function_bodies(ctx, ctx.ast.block);;

  m_symbol_table.exit_scope();
}

void Semantics::process_block(Context& ctx, const NodeBlock& block, const bool add_scope)
{
  auto is_statement = [](const std::unique_ptr<AstNode>& node)
  {
    return  node->node_type() == NodeType::VariableDecl ||
            node->node_type() == NodeType::VariableAssign ||
            node->node_type() == NodeType::ConditionIf ||
            node->node_type() == NodeType::Expression;
  };

  try
  {
    if (add_scope) {
      m_symbol_table.enter_scope(&block);
    }

    for(const auto& node : block.nodes | vw::filter(is_statement))
    {
      switch (node->node_type())
      {
        case NodeType::VariableDecl:
          process_variable_declaration(ctx, block, dynamic_cast<const VariableDecl&>(*node));
          break;

        case NodeType::VariableAssign:
          process_variable_assign(ctx, block, dynamic_cast<const VariableAssign&>(*node));
          break;

        case NodeType::ConditionIf:
          process_condition(ctx, *node, block, dynamic_cast<const ConditionIf&>(*node));
          break;

        case NodeType::Expression:
          handle_expression(ctx, *node, block, dynamic_cast<const Expression&>(*node));
          break;

        case NodeType::NodeBlock:
          process_block(ctx, dynamic_cast<const NodeBlock&>(*node));
          break;

        default:
          std::unreachable();
          break;
      }
    }
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    issue::compiler_error(ctx.issues, block);
  }

  if (add_scope) {
    m_symbol_table.exit_scope();
  }
}

// Functions //

void Semantics::process_function_signatures(Context& ctx)
{
  walk_nodes<FunctionDef>(ctx.ast.block.nodes, [&, this](const FunctionDef& def)
  {
    // overloading not permitted yet
    if (m_symbol_table.has_function(def.name))
    {
      issue::func_duplicate(ctx.issues, def, def.name);
      return;
    }

    std::vector<ResolvedSymbol> resolved_params;
    for (const auto& param : def.params)
    {
      if (const auto type = m_symbol_table.get_type(param.type_name); type) {
        resolved_params.emplace_back(param.param_name, *type);
      }
      else
      {
        issue::unknown_param_type(ctx.issues, param);
        return ;
      }
    }

    const auto return_type = m_symbol_table.get_type(def.return_type);
    if (!return_type) {
      issue::unknown_return_type(ctx.issues, def.name, def.return_type);
    }
    else {
      m_symbol_table.add_function(def, std::move(resolved_params), *return_type);
    }
  });
}

void Semantics::process_function_bodies(Context& ctx, const NodeBlock& block)
{
  walk_nodes<FunctionDef>(block.nodes, [this, &ctx](const FunctionDef& def)
  {
    process_block(ctx, def.body);
  });
}

void Semantics::process_variable_declaration(Context& ctx, const NodeBlock& block, const VariableDecl& decl)
{
  if (m_symbol_table.scope_has_variable(decl.var_name, block))
  {
    issue::var_duplicate(ctx.issues, decl);
    return;
  }

  VarType var_type, init_type;

  if (decl.has_explicit_type())
  {
    if (const auto type = m_symbol_table.get_type(decl.var_type); !type) {
      issue::unknown_variable_type(ctx.issues, decl);
      return;
    }
    else if (type == BuiltInType::Void) {
      issue::var_init_void(ctx.issues, decl);
    }
    else {
      var_type = *type;
    }
  }

  if (decl.has_initialiser())
  {
    if (const auto& init = *decl.initialiser; init.expr_type() == ExpressionType::FuncCall) {
      init_type = process_expression(ctx, decl, block, dynamic_cast<const FunctionCall&>(init));
    }
    else if (init.expr_type() == ExpressionType::Binary) {
      init_type = process_expression(ctx, decl, block, dynamic_cast<const BinaryExpression&>(init));
    }
    else {
      init_type = init.get_var_type(); // literal expression
    }

    if (init_type == BuiltInType::Void) {
      issue::var_init_void(ctx.issues, decl);
      return;
    }
    else if (init_type == unresolved_t) {
      return;
    }
  }

  if (decl.has_initialiser() && decl.has_explicit_type() && var_type != init_type) {
    issue::var_init_type_differ(ctx.issues, decl);
  }
  else {
    m_symbol_table.add_variable(decl.var_name, decl.has_initialiser() ? init_type : var_type);
  }
}

void Semantics::process_variable_assign(Context& ctx, const NodeBlock& block, const VariableAssign& assign)
{
  if (!m_symbol_table.has_visible_variable(assign.name, block))
  {
    issue::var_unknown(ctx.issues, assign, assign.name);
    return;
  }

  const auto var_type = m_symbol_table.get_variable(assign.name, block).type;
  const auto assign_type = handle_expression(ctx, assign, block, *assign.expr);

  if (assign_type != var_type)
  {
    issue::var_assign_type_differ(ctx.issues, assign);
  }
}

// Expressions //

VarType Semantics::process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const FunctionCall& call)
{
  if (!call.module.empty() && !Modules::exist(call.module))
  {
    issue::module_not_exist(ctx.issues, call, call.module);
    return unresolved_t;
  }

  if (!m_symbol_table.has_function(call.name))
  {
    issue::func_not_exist(ctx.issues, call, call.name);
    return unresolved_t;
  }

  const auto& resolved = m_symbol_table.get_function(call.name);

  if (call.args.size() != resolved.params.size())
  {
    issue::func_arg_count(ctx.issues, call);
    return unresolved_t;
  }

  for (uint8_t i = 0 ; i < call.args.size() ; ++i)
  {
    const auto param_type = resolved.params[i].type.value_as<BuiltInType>();
    if (param_type)
    {
      const auto& arg = *call.args[i];

      if (arg.is_expr_type(ExpressionType::FuncCall))
      {
        const auto& func_call = dynamic_cast<const FunctionCall&>(arg);

        if (const auto ret_type = process_expression(ctx, call, block, func_call); !ret_type) {
          return unresolved_t;
        }
        else if (ret_type == BuiltInType::Void)
        {
          issue::func_arg_void(ctx.issues, arg, resolved.params[i].name);
          return unresolved_t;
        }
        else if (ret_type != *param_type)
        {
          issue::func_arg(ctx.issues, arg, resolved.params[i].name);
          return unresolved_t;
        }
      }
      else if (arg.is_expr_type(ExpressionType::Binary))
      {
        const auto& expr = dynamic_cast<const BinaryExpression&>(arg);
        if (!process_expression(ctx, parent, block, expr))
          return unresolved_t;
      }
      else if (arg.is_expr_type(ExpressionType::VariableRef))
      {
        const auto& var = dynamic_cast<const VariableRef&>(arg);
        const auto var_type = process_expression(ctx, call, block, var);

        // if the variable is unknown, process_expression() will create an isue and
        // return unresolved_t
        if (var_type == unresolved_t) {
          return unresolved_t;
        }
        else if (var_type != *param_type)
        {
          issue::func_arg(ctx.issues, call, resolved.params[i].name);
          return unresolved_t;
        }
      }
      else if (!arg.is_convertible_to(*param_type))
      {
        issue::func_arg(ctx.issues, call, resolved.params[i].name);
        return unresolved_t;
      }
    }
  }

  return resolved.return_type;
}

VarType Semantics::process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const VariableRef& expr)
{
  if (!m_symbol_table.has_visible_variable(expr.name, block))
  {
    issue::var_unknown(ctx.issues, parent, expr.name);
    return unresolved_t;
  }
  else {
    return m_symbol_table.get_variable(expr.name, block).type;
  }
}

/*
 *
/// Returns the return value type of a binary expressions.
/// Determines the return type of lhs and rhs then calls process_operator()
/// which determines if those are valid for the operator.
///
/// Nested binary expressions, example:
///   a := foo1() + foo2() + foo3()
///
///  lhs = (foo1() + foo2())
///  rhs = foo3()
///
/// - Calls `process_expression(Context&, const AstNode&, BinaryExpression)` to get the result type of `foo1()+foo2()`
/// - Calls `process_expression(Context&, const AstNode&, FunctionCall)` to get the result type of `foo3()`
/// - Calls `process_operator()` with the types
*/
VarType Semantics::process_expression(Context& ctx, const AstNode& parent, const NodeBlock& block, const BinaryExpression& expr)
{
  const auto& lhs = *expr.lhs;
  const auto& rhs = *expr.rhs;

  VarType lhs_type = handle_expression(ctx, parent, block, lhs);
  VarType rhs_type = handle_expression(ctx, parent, block, rhs);

  const bool have_types = lhs_type != unresolved_t && rhs_type  != unresolved_t;
  return have_types ? process_operator(ctx, lhs_type, rhs_type, expr) :  unresolved_t ;
}

/// Determines the validity of the BinaryExpression::op on the operands, and returns
/// the return type (or unresolved_t if invalid).
VarType Semantics::process_operator(Context& ctx, const VarType& a, const VarType& b, const BinaryExpression& expr)
{
  if (a == unresolved_t) {
    throw std::runtime_error{"process_operator() encountered unresolved VarType"};
  }

  using enum BinaryOperator;
  using enum BuiltInType;

  VarType result_type{};
  bool valid{};

  // check for Void first, to highlight that, rather than differing types
  if (a == Void || b == Void) {
    issue::binary_operands_void(ctx.issues, expr);
  }
  else if (a != b) {
    issue::binary_operands_invalid(ctx.issues, expr);
  }
  else
  {
    switch (expr.op)
    {
      case Add:
      case Subtract:
      case Multiply:
      case Divide:
        valid = a == Int || a == Decimal || a == String;
        result_type = a;
        break;

      case Equal:
      case NotEqual:
        valid = true;
        result_type = Bool;
        break;

      case Less:
      case Greater:
      case LessEqual:
      case GreaterEqual:
        valid = !(a == Bool || a == String);
        result_type = Bool;
        break;
    }

    if (!valid) {
      issue::binary_operands_invalid(ctx.issues, expr);
    }
  }

  return valid ? result_type : unresolved_t;
}


// Conditions //

void Semantics::process_condition(Context& ctx, const AstNode& parent, const NodeBlock& block, const ConditionIf& cond)
{
  auto handle_condition_body = [&](const ConditionIf::ConditionBody& cb)
  {
    const auto cond_if_result = handle_expression(ctx, parent, block, dynamic_cast<const Expression&>(*cb.condition));

    if (cond_if_result != BuiltInType::Bool)
    {
      issue::condition_not_bool(ctx.issues, *cb.condition);
      return false;
    }
    else {
      process_block(ctx, cb.body);
    }
    return true;
  };

  // handle: if (condition) { block }
  if (handle_condition_body(cond.if_))
  {
    // handle each: else if (condition) { block }
    for (const auto& else_if : cond.else_if) {
      handle_condition_body(else_if);
    }

    // handle: else { block }
    if (cond.else_body) {
      process_block(ctx, *cond.else_body);
    }
  }
}
