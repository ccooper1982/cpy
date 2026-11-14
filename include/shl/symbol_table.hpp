#pragma once


#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <stack>
#include <string_view>
#include <vector>

#include <shl/ast/ast.hpp>
#include <shl/exec/core.hpp>
#include <shl/exec/proc.hpp>
#include <shl/functional.hpp>


struct Scope
{
  explicit Scope(Scope * parent)
    : parent(parent)
  {}

  Scope * parent{};
  ResolvedVariableMap vars{};
};


/// Maps function:
///  return type, param types
/// Maps variables:
///   type, scope visibility
///
/// Variable scope is essentially parent/child:
///  ```
///   a: bool;
///   b: int;
///
///   if (a)
///   {
///     c: int;
///     if (a > b) # a and b are visible from the parent scope
///     {
///       c: str;  # a new 'c' variable
///     }
///   }
///  ```
/// A stack is used to manage the parent/child scope tracking.
/// A new scope is created with enter_scope() (pushes onto the stack), whilst
/// exit_scope() pops from the stack.
class SymbolTable
{
  inline static const std::map<const std::string_view, const NativeType> NativeTypes = {
    {"int",   NativeType::Int},
    {"dec",   NativeType::Decimal},
    {"bool",  NativeType::Bool},
    {"str",   NativeType::String},
    {"void",  NativeType::Void}
  };

public:
  SymbolTable() = default;
  ~SymbolTable() = default;

  SymbolTable(const SymbolTable&) = delete;
  SymbolTable& operator=(const SymbolTable&) = delete;
  SymbolTable(SymbolTable&&) = default;
  SymbolTable& operator=(SymbolTable&&) = default;


  void add_function (const FunctionDef& def, std::vector<ResolvedSymbol> params, VarType return_type);

  bool has_function(const std::string_view name) const
  {
    return m_resolved_functions.contains(name);
  }

  bool has_native_function(const std::string_view name) const
  {
    return NativeCoreFunctions.contains(name) || NativeProcFunctions.contains(name);
  }

  void add_variable(const std::string_view name, const VarType type, const uint32_t position);

  void add_variable(const FunctionParam& param, const uint32_t position)
  {
    add_variable(param.param_name, *get_type(param.type_name), position);
  }

  // Check the current scope for variable. Does not check parents.
  bool scope_has_variable (const std::string_view name, const NodeBlock& block) const
  {
    return get_resolved_variable(name, &block, false) != nullptr;
  }

  // Checks if a variable is visible from the given block.
  // If not present in `block`, checks parents, until the variable is found.
  // Returns false if not found.
  bool has_visible_variable(const std::string_view name, const NodeBlock& block, const uint32_t position) const;

  std::optional<NativeType> get_type(const std::string_view type_name) const;

  const ResolvedFunction& get_function(const std::string_view name) const;

  size_t get_native_function_min_args(const std::string_view name) const
  {
    return get_native_function(name).min_args;
  }

  const NativeFunctionDef& get_native_function(const std::string_view name) const
  {
    auto get_native = [&name](const std::map<std::string_view, NativeFunctionDef>& natives) -> auto
    {
      return natives.find(name);
    };

    if (auto it = get_native(NativeCoreFunctions); it != rg::end(NativeCoreFunctions)) {
      return it->second;
    }
    else if (it = get_native(NativeProcFunctions); it != rg::end(NativeProcFunctions)) {
      return it->second;
    }

    throw std::runtime_error{std::format("get_function() called unexpectedly with: {}", name)};
  }

  /// Gets a resolved variable, checking the NodeBlock's scope, then checking each
  /// parent until it is found.
  /// Throws if the variable does not exist: has_visible_variable() should return
  /// true prior to calling get_variable().
  const ResolvedSymbol& get_variable(const std::string_view name, const NodeBlock& block) const;

  std::optional<ResolvedVariableMap> get_variables (const NodeBlock& block) const;

  void enter_scope(const NodeBlock * node);
  void exit_scope();

  void dump (std::ostream& os) const;


private:
  const ResolvedSymbol * get_resolved_variable(const std::string_view name, const NodeBlock * block, const bool visit_parents) const;



private:
  std::map<std::string_view, const ResolvedFunction> m_resolved_functions;
  std::map<const NodeBlock *, std::unique_ptr<Scope>> m_scopes;
  std::stack<Scope*> m_scope_tracker;
};
