#pragma once

#include <concepts>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <set>

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/ast/ast_node.hpp>
#include <string_view>


struct ResolvedParam
{
  std::string_view name;
  VarType type;
};

struct ResolvedTypes
{
  std::vector<ResolvedParam> params;
  VarType return_type;
};

class ResolvedTypesTable
{
  inline static const std::map<const std::string_view, const BuiltInType> BuiltIntTypes = {
    {"int", BuiltInType::Int},
    {"dec", BuiltInType::Decimal},
    {"bool", BuiltInType::Bool},
    {"str", BuiltInType::String},
    {"void", BuiltInType::Void}
  };

public:
  void add_function (const std::string_view name, std::vector<ResolvedParam> params, VarType return_type)
  {
    m_function_table.emplace(name, ResolvedTypes{.params = std::move(params), .return_type = return_type});
  }

  bool function_exists(const std::string_view name)
  {
    return m_function_table.contains(name);
  }

  std::optional<BuiltInType> get_type(const std::string_view t)
  {
    const auto it = BuiltIntTypes.find(t) ;
    if (it == rg::end(BuiltIntTypes))
     return std::nullopt;

    return it->second;
  }

  const ResolvedTypes& get_function_types(const std::string_view func) const
  {
    return m_function_table.find(func)->second;
  }

private:
  std::map<std::string_view, const ResolvedTypes> m_function_table;
};


class Semantics
{
  struct Context
  {
    Context(SourceFile& src, Issues& issues) : ast(src), issues(issues)
    {}

    SourceFile& ast;
    Issues& issues;
  };

public:
  void process(Script& script);

private:
  template<typename NodeT>
  using NodeHandler = std::function<void(const NodeT&)>;

  void process_function_defs(Context& ctx);

  void process_function_calls(Context& ctx);
  void process_function_call(Context& ctx, const FunctionCall& call);


  template<typename NodeT, typename Handler>
    requires std::derived_from<NodeT, AstNode> &&
             std::convertible_to<decltype(NodeT::Type), NodeType>
  void walk_nodes(const SourceFile& root, Handler&& handler)
  {
    const constexpr NodeType nt = NodeT::Type;

    auto by_node_type = [](const std::unique_ptr<AstNode>& n) {
      return n->is_node_type(nt);
    };

    for (const auto& node : root.nodes | vw::filter(by_node_type)) {
      handler(dynamic_cast<const NodeT&>(*node));
    }
  }

  template<typename ExprT, typename Handler>
    requires std::derived_from<ExprT, Expression> &&
             std::convertible_to<decltype(ExprT::ExprType), ExpressionType>
  void walk_expressions(const SourceFile& root, Handler&& handler)
  {
    const constexpr ExpressionType et = ExprT::ExprType;

    walk_nodes<Expression>(root, [&](const Expression& expr_node)
    {
      if (expr_node.is_expr_type(et)) {
        handler(dynamic_cast<const ExprT&>(expr_node));
      }
    });
  }

private:
  ResolvedTypesTable m_resolved_table;
  std::set<std::string_view> m_function_names;
};
