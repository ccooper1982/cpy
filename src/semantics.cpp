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
}

void Semantics::process_function_defs(Context& ctx)
{
  walk_nodes<FunctionDef>(ctx.ast, [&, this](const FunctionDef& def)
  {
    std::vector<ResolvedParam> resolved_params;
    bool error{};

    for (const auto& param : def.params)
    {
      if (const auto type = m_resolved_table.get_type(param.type_name); type) {
        resolved_params.emplace_back(param.param_name, *type);
      }
      else
      {
        issue::unknown_param_type(ctx.issues, param);
        error = true;
        break;
      }
    }

    const auto return_type = m_resolved_table.get_type(def.return_type);
    if (!error && !return_type)
    {
      error = true;
      issue::unknown_return_param_type(ctx.issues, def.name, def.return_type);
    }

    if (!error) {
      m_resolved_table.add_function(def.name, std::move(resolved_params), *return_type);
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
    else if (!m_resolved_table.function_exists(call.name)) {
      issue::func_not_exist(ctx.issues, call, call.name);
    }
    else {
      process_function_call(ctx, call);
    }
  });
}

void Semantics::process_function_call(Context& ctx, const FunctionCall& call)
{
  const auto& resolved = m_resolved_table.get_function_types(call.name);

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
