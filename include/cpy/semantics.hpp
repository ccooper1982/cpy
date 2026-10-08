#pragma once

#include <concepts>
#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <ostream>
#include <ranges>
#include <set>

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/ast/ast_node.hpp>
#include <string_view>


struct ResolvedSymbol
{
  std::string_view name;
  VarType type;
};

struct ResolvedFunction
{
  std::vector<ResolvedSymbol> params;
  VarType return_type;
};


class SymbolTable
{
  inline static const std::map<const std::string_view, const BuiltInType> BuiltIntTypes = {
    {"int",   BuiltInType::Int},
    {"dec",   BuiltInType::Decimal},
    {"bool",  BuiltInType::Bool},
    {"str",   BuiltInType::String},
    {"void",  BuiltInType::Void}
  };

public:
  SymbolTable() = default;
  ~SymbolTable() = default;

  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;
  SymbolTable(SymbolTable&&) = default;
  SymbolTable& operator=(SymbolTable&&) = default;


  void add_function (const std::string_view name, std::vector<ResolvedSymbol> params, VarType return_type)
  {
    m_functions.emplace(name, ResolvedFunction{.params = std::move(params), .return_type = return_type});
  }

  void add_variable(const std::string_view name, VarType type)
  {
    m_vars.emplace(name, ResolvedSymbol{.name = name, .type = type});
  }

  bool have_variable(const std::string_view name) const
  {
    return m_vars.contains(name);
  }

  bool have_function(const std::string_view name) const
  {
    return m_functions.contains(name);
  }

  std::optional<BuiltInType> get_type(const std::string_view t) const
  {
    const auto it = BuiltIntTypes.find(t) ;
    if (it == rg::end(BuiltIntTypes))
     return std::nullopt;

    return it->second;
  }

  const ResolvedFunction& get_function(const std::string_view func) const
  {
    return m_functions.find(func)->second;
  }

  const ResolvedSymbol& get_variable(const std::string_view var) const
  {
    return m_vars.find(var)->second;
  }

  void dump (std::ostream& os) const
  {
    os << "-- Functions --\n";
    for(const auto& [name, resolved] : m_functions)
    {
      os << name << '\n';
      os << "  - return: "<< to_string(resolved.return_type) << '\n' ;
      os << "  - params:\n";
      for(const auto& symbol : resolved.params) {
        os << std::setw(8) << symbol.name << " : " << to_string(symbol.type) << '\n';
      }
    }

    os << "\n-- Variables --\n";
    for(const auto& [name, resolved] : m_vars) {
      os << name << std::setw(4) << '|' << to_string(resolved.type) << '\n';
    }
  }

private:
  std::map<std::string_view, const ResolvedFunction> m_functions;
  std::map<std::string_view, const ResolvedSymbol> m_vars;
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
  const SymbolTable& symbol_table() const { return m_symbol_table; }

private:
  template<typename NodeT>
  using NodeHandler = std::function<void(const NodeT&)>;

  // functions
  void process_function_defs(Context& ctx);
  void process_function_calls(Context& ctx);
  bool process_function_call(Context& ctx, const FunctionCall& call);

  // variables
  void process_variable_declarations(Context& ctx);

  // utils
  template<typename NodeT, typename Handler>
    requires std::derived_from<NodeT, AstNode> &&
             std::convertible_to<decltype(NodeT::Type), NodeType>
  void walk_nodes(const std::vector<std::unique_ptr<AstNode>>& nodes, Handler&& handler)
  {
    const constexpr NodeType nt = NodeT::Type;

    auto by_node_type = [](const std::unique_ptr<AstNode>& n) {
      return n->is_node_type(nt);
    };

    for (const auto& node : nodes | vw::filter(by_node_type)) {
      handler(dynamic_cast<NodeT&>(*node));
    }
  }

  template<typename ExprT, typename Handler>
    requires std::derived_from<ExprT, Expression> &&
             std::convertible_to<decltype(ExprT::ExprType), ExpressionType>
  void walk_expressions(const SourceFile& root, Handler&& handler)
  {
    const constexpr ExpressionType et = ExprT::ExprType;

    walk_nodes<Expression>(root.nodes, [&](Expression& expr_node)
    {
      if (expr_node.is_expr_type(et)) {
        handler(dynamic_cast<ExprT&>(expr_node));
      }
    });
  }

private:
  SymbolTable m_symbol_table;
};
