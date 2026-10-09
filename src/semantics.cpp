#include "cpy/ast/ast_node.hpp"
#include "cpy/issues.hpp"
#include <cpy/semantics.hpp>
#include <cpy/modules.hpp>

#include <optional>
#include <stdexcept>
#include <string_view>


// Semantic checker
//  │
//  ├── type resolution
//  ├── symbol/function lookup
//  ├── type checking
//  └── diagnostics

static constexpr const auto unresolved_t = BuiltInType::Unknown;


void Semantics::process(Script& script)
{
  if (!(script.ast && script.issues)) {
    throw std::runtime_error{"Ast and/or Issues not allocated"};
  }

  Context ctx(*script.ast, *script.issues);

  process_function_defs(ctx);

  process_variable_declarations(ctx);

  process_function_calls(ctx);
}

void Semantics::process_function_defs(Context& ctx)
{
  walk_nodes<FunctionDef>(ctx.ast.nodes, [&, this](const FunctionDef& def)
  {
    // overloading not permitted yet
    if (m_symbol_table.have_function(def.name)) {
      issue::func_duplicate(ctx.issues, def, def.name);
    }

    std::vector<ResolvedSymbol> resolved_params;
    bool error{};

    for (const auto& param : def.params)
    {
      if (const auto type = m_symbol_table.get_type(param.type_name); type) {
        resolved_params.emplace_back(param.param_name, *type);
      }
      else
      {
        issue::unknown_param_type(ctx.issues, param);
        error = true;
        break;
      }
    }

    const auto return_type = m_symbol_table.get_type(def.return_type);
    if (!error && !return_type)
    {
      error = true;
      issue::unknown_return_type(ctx.issues, def.name, def.return_type);
    }

    if (!error) {
      m_symbol_table.add_function(def.name, std::move(resolved_params), *return_type);
    }
  });
}

void Semantics::process_function_calls(Context& ctx)
{
  walk_expressions<FunctionCall>(ctx.ast, [this, &ctx](const FunctionCall& call)
  {
    if (!call.module.empty() && !Modules::exist(call.module)) {
      issue::module_not_exist(ctx.issues, call, call.module);
    }
    else {
      process_expression(ctx, ctx.ast, call);
    }
  });
}

void Semantics::process_variable_declarations(Context& ctx)
{
  auto check_vars = [&ctx, this](const std::vector<std::unique_ptr<AstNode>>& nodes)
  {
    walk_nodes<VariableDecl>(nodes, [this, &ctx](const VariableDecl& decl)
    {
      if (m_symbol_table.have_variable(decl.var_name))
      {
        issue::var_duplicate(ctx.issues, decl, decl.var_name);
        return;
      }

      if (decl.has_explicit_type())
      {
        if (const auto type = m_symbol_table.get_type(decl.var_type); !type) {
          issue::unknown_variable_type(ctx.issues, decl);
        }
        else {
          m_symbol_table.add_variable(decl.var_name, *type);
        }
      }
      else
      {
        if (const auto& init = *decl.initialiser; init.expr_type() == ExpressionType::FuncCall)
        {
          const auto& call = dynamic_cast<const FunctionCall&>(init);
          if (const auto ret_type = process_expression(ctx, decl, call); ret_type)
          {
            if (ret_type == BuiltInType::Void) {
              issue::var_init_void(ctx.issues, decl);
            }
            else {
              m_symbol_table.add_variable(decl.var_name, ret_type);
            }
          }
        }
        else if (init.expr_type() == ExpressionType::Binary)
        {
          const auto& expr = dynamic_cast<const BinaryExpression&>(init);
          const auto [lhs_type, rhs_type] = process_expression(ctx, decl, expr);

          if (lhs_type != rhs_type) {
            issue::var_init_binary_differ(ctx.issues, decl);
          }
          else if (lhs_type != BuiltInType::Unknown && rhs_type != BuiltInType::Unknown) {
            m_symbol_table.add_variable(decl.var_name, lhs_type);
          }
        }
        else {
          m_symbol_table.add_variable(decl.var_name, init.get_var_type());
        }
      }
    });
  };

  // top level
  check_vars(ctx.ast.nodes);

  // declarations within functions
  walk_nodes<FunctionDef>(ctx.ast.nodes, [&check_vars](const FunctionDef& func_def)
  {
    check_vars(func_def.body.nodes);
  });
}

VarType Semantics::process_expression(Context& ctx, const AstNode& parent, const FunctionCall& call)
{
  if (!m_symbol_table.have_function(call.name))
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

        if (const auto ret_type = process_expression(ctx, call, func_call); !ret_type) {
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
        auto [lhs_type, rhs_type] = process_expression(ctx, parent, expr);

        if (lhs_type != rhs_type)
        {
          issue::func_arg_binary_differ(ctx.issues, arg);
          return unresolved_t;
        }
      }
      else if (arg.is_expr_type(ExpressionType::VariableRef))
      {
        const auto& var = dynamic_cast<const VariableRef&>(arg);
        const auto var_type = process_expression(ctx, call, var);

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

VarType Semantics::process_expression(Context& ctx, const AstNode& parent, const VariableRef& expr)
{
  if (!m_symbol_table.have_variable(expr.name))
  {
    issue::var_unknown(ctx.issues, parent, expr.name);
    return unresolved_t;
  }
  else {
    return m_symbol_table.get_variable(expr.name).type;
  }
}

std::pair<VarType,VarType> Semantics::process_expression(Context& ctx, const AstNode& parent, const BinaryExpression& expr)
{
  const auto& lhs = *expr.lhs;
  const auto& rhs = *expr.rhs;

  if (lhs.is_expr_type(ExpressionType::Binary) || rhs.is_expr_type(ExpressionType::Binary)) {
    issue::unsupported(ctx.issues, expr, "Nested binary expressions");
    return {unresolved_t, unresolved_t};
  }

  VarType lhs_type, rhs_type;

  if (lhs.is_expr_type(ExpressionType::FuncCall)){
    lhs_type = process_expression(ctx, parent, dynamic_cast<const FunctionCall&>(lhs));
  }
  else if (lhs.is_expr_type(ExpressionType::VariableRef)){
    lhs_type = process_expression(ctx, parent, dynamic_cast<const VariableRef&>(lhs));
  }
  else {
    lhs_type = lhs.get_var_type();
  }

  if (rhs.is_expr_type(ExpressionType::FuncCall)) {
    rhs_type = process_expression(ctx, parent, dynamic_cast<const FunctionCall&>(rhs));
  }
  else if (rhs.is_expr_type(ExpressionType::VariableRef)){
    rhs_type = process_expression(ctx, parent, dynamic_cast<const VariableRef&>(rhs));
  }
  else {
    rhs_type = rhs.get_var_type();
  }

  return {lhs_type, rhs_type};
}
