#pragma once

#include <algorithm>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
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

  using VarPair = decltype(vars)::value_type;
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
    m_resolved_functions.emplace(name, ResolvedFunction{.params = std::move(params), .return_type = return_type});
  }

  bool has_function(const std::string_view name) const
  {
    return m_resolved_functions.contains(name);
  }

  void add_variable(const std::string_view name, VarType type)
  {
    if (m_scope_tracker.empty())
      throw std::runtime_error{"SymbolTable::add_variable(): scope tracker is empty"};

    m_scope_tracker.top()->vars.emplace(name, ResolvedSymbol{.name = name, .type = type});
  }

  // Check the current scope for variable. Does not check parents.
  bool scope_has_variable (const std::string_view name, const NodeBlock& block) const
  {
    return get_resolved_variable(name, &block, false) != nullptr;
  }

  // Checks if a variable is visible from the given block.
  // If not present in `block`, checks parents, until the variable is found.
  // Returns false if not found.
  bool has_visible_variable(const std::string_view name, const NodeBlock& block) const
  {
    return get_resolved_variable(name, &block, true) != nullptr;
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

  // Gets a resolved variable, checking the NodeBlock's scope, then checking each parent until
  // it is found.
  // Should not be called if the variable does not exist (has_visible_variable() should return
  // true prior to calling this).
  const ResolvedSymbol& get_variable(const std::string_view name, const NodeBlock& block) const
  {
    if (const auto * symbol = get_resolved_variable(name, &block, true); symbol)
      return *symbol;

    throw std::runtime_error{"SymbolTable::get_variable() - var does not exist"};
  }

  void enter_scope(const NodeBlock * node)
  {
    const auto parent = m_scope_tracker.empty() ? nullptr : m_scope_tracker.top();
    auto [it, emplaced] = m_scopes.emplace(node, std::make_unique<Scope>(parent));

    if (!emplaced)
      std::cout << "Scope already exists for NodeBlock " << node << '\n';

    m_scope_tracker.push(it->second.get());
  }

  void exit_scope()
  {
    if (!m_scope_tracker.empty()) {
      m_scope_tracker.pop();
    }
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
    os << "\n-- Variables --\n";

    for (const auto& [block, scope] : m_scopes)
    {
      std::cout << "Block: [" << block->source.start << ", " << block->source.end <<  "]\n";
      for (const auto& [var, resolved] : scope->vars) {
        std::cout << resolved.name << ":" << to_string(resolved.type) << "\n";
      }
    }
  }


private:
  const ResolvedSymbol * get_resolved_variable(const std::string_view name, const NodeBlock * block, const bool visit_parents) const
  {
    auto in_scope = [&name](const Scope * scope) -> const ResolvedSymbol*
    {
      const auto it = scope->vars.find(name);
      return it == scope->vars.cend() ? nullptr : &it->second;
    };

    const auto it_scopes = m_scopes.find(block) ;
    if (it_scopes == m_scopes.cend()) {
      return nullptr;
    }

    // check NodeBlock's scope
    const Scope * scope = it_scopes->second.get();
    const auto resolved = in_scope(scope);

    if (!visit_parents || resolved) {
      return resolved;
    }

    // travel up parents
    for (scope = scope->parent; scope ; scope = scope->parent) {
      if (const auto resolved = in_scope(scope); resolved)
        return resolved;
    }
    return nullptr;
  }

private:
  std::map<std::string_view, const ResolvedFunction> m_resolved_functions;
  std::map<const NodeBlock *, std::unique_ptr<Scope>> m_scopes;
  std::stack<Scope*> m_scope_tracker;
};
