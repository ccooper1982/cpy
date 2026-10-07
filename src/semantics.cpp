#include "cpy/ast/ast_node.hpp"
#include "cpy/issues.hpp"
#include <cpy/semantics.hpp>
#include <cpy/modules.hpp>

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
    else if (!m_symbol_table.have_function(call.name)) {
      issue::func_not_exist(ctx.issues, call, call.name);
    }
    else {
      process_function_call(ctx, call);
    }
  });
}

void Semantics::process_function_call(Context& ctx, const FunctionCall& call)
{
  const auto& resolved = m_symbol_table.get_function(call.name);

  if (call.args.size() != resolved.params.size())
  {
    issue::func_args_count(ctx.issues, call);
    return;
  }

  for (uint8_t i = 0 ; i < call.args.size() ; ++i)
  {
    const auto builtin_type = resolved.params[i].type.value_as<BuiltInType>();
    if (builtin_type)
    {
      if (!call.args[i]->is_convertible_to(*builtin_type)) {
        issue::func_args(ctx.issues, *(call.args[i]), call.name, resolved.params[i].name);
      }
    }
  }
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
        // TODO over simplifies, needs more thought for binary expression initialiser:
        //  a := foo() + bar();
        if (const auto& init = *decl.initialiser; init.expr_type() == ExpressionType::FuncCall)
        {
          const auto& call = dynamic_cast<const FunctionCall&>(init);

          if (!m_symbol_table.have_function(call.name)) {
            issue::func_not_exist(ctx.issues, decl, call.name);
          }
          else
          {
            const auto& types = m_symbol_table.get_function(call.name);
            if (types.return_type == BuiltInType::Void) {
              issue::var_init_void(ctx.issues, decl, decl.var_name);
            }
            else {
              m_symbol_table.add_variable(decl.var_name, types.return_type);
            }
          }
        }
        else if (init.expr_type() == ExpressionType::Binary)
        {
          const auto& expr = dynamic_cast<const BinaryExpression&>(init);
          const auto& lhs = *expr.lhs;
          const auto& rhs = *expr.rhs;

          if (lhs.is_expr_type(ExpressionType::FuncCall) || rhs.is_expr_type(ExpressionType::FuncCall)) {
            issue::unsupported(ctx.issues, decl, "Initialise from this binary expression");
          }
          else if (lhs.expr_type() != rhs.expr_type()) {
            issue::var_init_binary_invalid(ctx.issues, decl, decl.var_name);
          }
          else {
            m_symbol_table.add_variable(decl.var_name, lhs.get_var_type());
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
