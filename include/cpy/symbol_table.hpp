#pragma once

#include <iomanip>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string_view>

#include <cpy/ast/ast_node.hpp>

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


  void add_resolved_function (const std::string_view name, std::vector<ResolvedSymbol> params, VarType return_type)
  {
    m_resolved_functions.emplace(name, ResolvedFunction{.params = std::move(params), .return_type = return_type});
  }

  void add_resolved_variable(const std::string_view name, VarType type)
  {
    m_resolved_vars.emplace(name, ResolvedSymbol{.name = name, .type = type});
  }

  bool have_resolved_variable(const std::string_view name) const
  {
    return m_resolved_vars.contains(name);
  }

  bool have_resolved_function(const std::string_view name) const
  {
    return m_resolved_functions.contains(name);
  }

  void add_function (const std::string_view name)
  {
    m_functions.emplace(name);
  }

  void add_variable (const std::string_view name)
  {
    m_vars.emplace(name);
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
    return m_resolved_functions.find(func)->second;
  }

  const ResolvedSymbol& get_variable(const std::string_view var) const
  {
    return m_resolved_vars.find(var)->second;
  }

  void dump (std::ostream& os) const
  {
    os << "-- Functions --\n";
    for(const auto& [name, resolved] : m_resolved_functions)
    {
      os << name << '\n';
      os << "  - return: "<< to_string(resolved.return_type) << '\n' ;
      os << "  - params:\n";
      for(const auto& symbol : resolved.params) {
        os << std::setw(8) << symbol.name << " : " << to_string(symbol.type) << '\n';
      }
    }

    os << "\n-- Variables --\n";
    for(const auto& [name, resolved] : m_resolved_vars) {
      os << name << std::setw(4) << '|' << to_string(resolved.type) << '\n';
    }
  }

private:
  std::map<std::string_view, const ResolvedFunction> m_resolved_functions;
  std::map<std::string_view, const ResolvedSymbol> m_resolved_vars;
  std::set<std::string_view> m_functions;
  std::set<std::string_view> m_vars;
};
