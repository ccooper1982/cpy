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

void Semantics::process(Script& script)
{
  if (!(script.ast && script.issues)) {
    throw std::runtime_error{"Ast and/or Issues not allocated"};
  }

  Context ctx(*script.ast, *script.issues);

  process_function_defs(ctx);

  process_function_calls(ctx);

  process_variable_declarations(ctx);
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
      process_function_call(ctx, call);
    }
  });
}

bool Semantics::process_function_call(Context& ctx, const FunctionCall& call)
{
  if (!m_symbol_table.have_function(call.name))
  {
    issue::func_not_exist(ctx.issues, call, call.name);
    return false;
  }

  const auto& resolved = m_symbol_table.get_function(call.name);

  if (call.args.size() != resolved.params.size())
  {
    issue::func_arg_count(ctx.issues, call);
    return false;
  }

  for (uint8_t i = 0 ; i < call.args.size() ; ++i)
  {
    const auto builtin_type = resolved.params[i].type.value_as<BuiltInType>();
    if (builtin_type)
    {
      const auto& call_arg = *call.args[i];

      if (call_arg.is_expr_type(ExpressionType::FuncCall))
      {
        const auto& arg_call = dynamic_cast<const FunctionCall&>(call_arg);

        if (process_function_call(ctx, arg_call))
        {
          const auto& resolved = m_symbol_table.get_function(arg_call.name);
          if (resolved.return_type == BuiltInType::Void) {
            issue::func_arg_void(ctx.issues, call_arg, resolved.params[i].name);
          }
          else if (resolved.return_type != *builtin_type) {
            issue::func_arg(ctx.issues, call_arg, arg_call.name, resolved.params[i].name);
          }
        }
      }
      else if (!call_arg.is_convertible_to(*builtin_type))
      {
        issue::func_arg(ctx.issues, call_arg, call.name, resolved.params[i].name);
        return false;
      }
    }
  }
  return true;
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
        auto get_type_from_func_call = [this, &ctx, &decl](const FunctionCall& call) -> std::optional<VarType>
        {
          if (process_function_call(ctx, call))
          {
            const auto& resolved = m_symbol_table.get_function(call.name);
            if (resolved.return_type != BuiltInType::Void) {
              return resolved.return_type;
            }
            else {
              issue::var_init_void(ctx.issues, decl, decl.var_name);
            }
          }

          return std::nullopt;
        };

        if (const auto& init = *decl.initialiser; init.expr_type() == ExpressionType::FuncCall)
        {
          const auto& call = dynamic_cast<const FunctionCall&>(init);

          if (auto type = get_type_from_func_call(call); type) {
            m_symbol_table.add_variable(decl.var_name, *type);
          }
        }
        else if (init.expr_type() == ExpressionType::Binary)
        {
          const auto& expr = dynamic_cast<const BinaryExpression&>(init);
          const auto& lhs = *expr.lhs;
          const auto& rhs = *expr.rhs;

          VarType lhs_type, rhs_type;

          if (lhs.is_expr_type(ExpressionType::FuncCall))
          {
            if (const auto type = get_type_from_func_call(dynamic_cast<const FunctionCall&>(lhs)); type) {
              lhs_type = *type;
            }
          }
          else {
            lhs_type = lhs.get_var_type();
          }

          if (rhs.is_expr_type(ExpressionType::FuncCall))
          {
            if (const auto type = get_type_from_func_call(dynamic_cast<const FunctionCall&>(rhs)); type) {
              rhs_type = *type;
            }
          }
          else {
            rhs_type = rhs.get_var_type();
          }

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
