#include "cpy/ast/ast_node.hpp"
#include "cpy/issues.hpp"
#include <cpy/semantics.hpp>
#include <cpy/modules.hpp>
#include <ranges>
#include <stdexcept>


// Semantic checker
//  │
//  ├── type resolution
//  ├── symbol/function lookup
//  ├── type checking
//  └── diagnostics


static bool param_arg_valid(const FunctionParam& def_param, const std::unique_ptr<Expression>& call_arg)
{
  return false;
//   const auto def_param_type = def_param.type.value_as<BuiltInType>();
//
//   if (!def_param_type)
//     throw std::runtime_error("Function has unsupported UserType parameter");
//
//   switch (*def_param_type)
//   {
//     using enum BuiltInType;
//     case Int:
//       return call_arg->is_convertible_to(BuiltInType::Int) ;
//
//     case String:
//       return call_arg->is_convertible_to(BuiltInType::String);
//
//     case Decimal:
//       return call_arg->is_convertible_to(BuiltInType::Decimal);
//
//     case Bool:
//       return call_arg->is_convertible_to(BuiltInType::Bool);
//
//     case Unknown:
//       return false;
//
//     default:
//       throw std::runtime_error("Function has unsupported BuiltInType parameter");
//       break;
//   }
}


void Semantics::process_function_call(const SourceFile& root, const FunctionCall& call, Issues& issues)
{
  auto by_func_name = [&name = call.name](const std::unique_ptr<AstNode>& n)
                      {
                        if (!n->is_node_type(NodeType::FunctionDef))
                          return false;

                        return dynamic_cast<const FunctionDef&>(*n).name == name;
                      };

  for (const auto& func_def_node : root.nodes | vw::filter(by_func_name))
  {
    const auto& def = dynamic_cast<FunctionDef&>(*func_def_node);
    const auto valid = rg::equal(def.params, call.args, [](const auto& param, const auto& arg)
                       {
                         return param_arg_valid(param, arg);
                       });
    if (!valid) {
      issue::func_args(issues, call, call.name);
    }
  }
}

void Semantics::process(Script& script)
{
  if (!(script.ast && script.issues)) {
    throw std::runtime_error{"Ast and/or Issues not allocated"};
  }

  auto by_expr_type = [](const ExpressionType et)
  {
    return [et](const std::unique_ptr<AstNode>& n)
    {
      if (!n->is_node_type(NodeType::Expression))
        return false;

      const auto& expr = dynamic_cast<const Expression&>(*n);
      return expr.is_expr_type(et);
    };
  };

  auto& src_file_node = *script.ast;
  auto& issues = *script.issues;

  create_cache(src_file_node);

  // function calls
  for (const auto& func_call_node : src_file_node.nodes | vw::filter(by_expr_type(ExpressionType::FuncCall)))
  {
    const auto& func_call = dynamic_cast<FunctionCall&>(*func_call_node);

    if (!func_call.module.empty() && !Modules::exist(func_call.module)) {
      issue::module_not_exist(issues, func_call, func_call.module);
    }
    else if (!m_function_names.contains(func_call.name)) {
      issue::func_not_exist(issues, func_call, func_call.name);
    }
    else {
      process_function_call(src_file_node, func_call, issues);
    }
  }
}

void Semantics::create_cache(const SourceFile& root)
{
  walk_ast<FunctionDef>(root, [this](const FunctionDef& node)
  {
    m_function_names.emplace(node.name);
  });

}
