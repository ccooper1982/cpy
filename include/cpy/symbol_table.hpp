#pragma once

#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <stack>
#include <stdexcept>
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

struct Scope
{
  explicit Scope(Scope * parent)
    : parent(parent)
  {}

  Scope * parent{};
  std::map<std::string_view, const ResolvedSymbol> vars{};
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
  SymbolTable()
  {
    m_scopes.push(std::make_unique<Scope>(nullptr));
  }

  ~SymbolTable() = default;

  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;
  SymbolTable(SymbolTable&&) = default;
  SymbolTable& operator=(SymbolTable&&) = default;


  void add_resolved_function (const std::string_view name, std::vector<ResolvedSymbol> params, VarType return_type)
  {
    m_resolved_functions.emplace(name, ResolvedFunction{.params = std::move(params), .return_type = return_type});
  }

  bool have_resolved_function(const std::string_view name) const
  {
    return m_resolved_functions.contains(name);
  }

  void add_resolved_variable(const std::string_view name, VarType type)
  {
    std::cout << "Add " << name << " to scope: " << m_scopes.size() - 1 << "\n";
    m_scopes.top()->vars.emplace(name, ResolvedSymbol{.name = name, .type = type});
  }

  // Check the current scope for the variable, if not present, go
  // to the parent and check again, until we reach root level.
  bool have_resolved_variable(const std::string_view name) const
  {
    for (auto curr = m_scopes.top().get(); curr; curr = curr->parent) {
      if (curr->vars.contains(name))
        return true;
    }
    return false;
  }

  bool scope_has_resolved_variable(const std::string_view name) const
  {
    return m_scopes.top()->vars.contains(name);
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

  const ResolvedSymbol& get_variable(const std::string_view name) const
  {
    for (auto curr = m_scopes.top().get(); curr; curr = curr->parent)
    {
      if (const auto it = curr->vars.find(name) ; it != rg::end(curr->vars)) {
        return it->second;
      }
    }
    throw std::runtime_error{"SymbolTable:;get_variable() - var does not exist"};
  }

  void enter_scope()
  {
    m_scopes.push(std::make_unique<Scope>(m_scopes.top().get()));
  }

  void exit_scope()
  {
    m_scopes.pop();
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

    // this can only dump variables at the top level because we use
    // a stack, so they are popped
//     os << "\n-- Variables --\n";
//
//     auto scope = m_scopes.top().get();
//     while (scope)
//     {
//       for(const auto& [name, resolved] : scope->vars) {
//         os << name << std::setw(4) << '|' << to_string(resolved.type) << '\n';
//       }
//       scope = scope->parent;
//     }
  }

private:
  std::map<std::string_view, const ResolvedFunction> m_resolved_functions;
  std::stack<std::unique_ptr<Scope>> m_scopes;
};
