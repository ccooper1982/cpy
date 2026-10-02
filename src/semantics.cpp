#include <cpy/semantics.hpp>
#include <cpy/modules.hpp>


// Semantic checker
//  │
//  ├── symbol/function lookup
//  ├── type checking
//  └── diagnostics

// bool does_function_exist(const SourceFile& src, const std::string_view name, const VarType return_type, const std::vector<FunctionParam>& params, const bool check_param_names = false)
// {
//   return rg::find_if(src.nodes, [&](const auto& node) {
//           if (!node->is_node_type(NodeType::FunctionDef))
//             return false;
//
//           const auto& func = dynamic_cast<const FunctionDef&>(*node);
//
//           if (func.return_type != return_type || func.name != name)
//             return false;
//
//           return check_param_names ? rg::equal(func.params, params, FunctionParamCmp{})
//                                    : rg::equal(func.params, params, FunctionParamCmpIgnoreName{});
//          }) != src.nodes.cend();
// }

// std::uint16_t count_function_definitions (const SourceFile& src, const std::string_view name)
// {
//   return rg::count_if(src.nodes, [&](const auto& node) {
//             if (!node->is_node_type(NodeType::FunctionDef))
//               return false;
//             return dynamic_cast<const FunctionDef&>(*node).name == name;
//          });
// }

// bool have_entry_point(const SourceFile& src)
// {
//   return does_function_exist(src, "main", BuiltInType::Int,  {FunctionParam{BuiltInType::String}}) &&
//          count_function_definitions(src, "main") == 1U;
// }

static bool param_arg_valid(const FunctionParam& def_param, const std::unique_ptr<Expression>& call_arg)
{
  const auto def_param_type = def_param.type.value_as<BuiltInType>();
  if (!def_param_type)
    throw std::runtime_error("Function has unsupported UserType parameter");

  switch (*def_param_type)
  {
    using enum BuiltInType;
    case Int:
      return call_arg->is_convertible_to(BuiltInType::Int) ;

    case String:
      return call_arg->is_convertible_to(BuiltInType::String);

    case Decimal:
      return call_arg->is_convertible_to(BuiltInType::Decimal);

    case Bool:
      return call_arg->is_convertible_to(BuiltInType::Bool);

    case Unknown:
      return false;

    default:
      throw std::runtime_error("Function has unsupported BuiltInType parameter");
      break;
  }
}


static bool func_call_valid(const FunctionDef& def, const FunctionCall& call)
{
  if (def.name != call.name) {
    return false;
  }

  return rg::equal(def.params, call.args, [](const auto& param, const auto& arg) {
      return param_arg_valid(param, arg);
    }
  );
}

static bool function_call_valid(const SourceFile& root, const FunctionCall& call)
{
  auto only_func_defs = [](const std::unique_ptr<AstNode>& n) {
    return n->is_node_type(NodeType::FunctionDef);
  };

  for (const auto& func_def_node : root.nodes | vw::filter(only_func_defs))
  {
    const auto& def = dynamic_cast<FunctionDef&>(*func_def_node);
    if (func_call_valid(def, call))
      return true;
  }
  return false;
}


void Semantics::process()
{
  auto by_node_type = [](const NodeType nt)
  {
    return [nt](const std::unique_ptr<AstNode>& n){ return n->is_node_type(nt); };
  };

  // function calls
  for (const auto& func_call_node : root.nodes | vw::filter(by_node_type(NodeType::FunctionCall)))
  {
    const auto& func_call = dynamic_cast<FunctionCall&>(*func_call_node);

    if (!func_call.module.empty() && !Modules::exist(func_call.module)) {
      create_issue_module_not_exist(issues, func_call, func_call.module);
    }
    else if (!function_call_valid(root, func_call)) {
      create_issue_func_not_exist(issues, func_call, func_call.name);
    }
  }
}
