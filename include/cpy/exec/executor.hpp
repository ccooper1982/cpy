#pragma once

#include "cpy/functional.hpp"
#include <cpy/ast/ast_node.hpp>
#include <cpy/ast/expressions.hpp>
#include <cpy/ast/ast.hpp>
#include <cpy/common.hpp>
#include <cpy/exec/operators.hpp>
#include <cpy/symbol_table.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>


inline static VarValue value_from_type (const VarType type)
{
  if (!type.is_type<NativeType>()) {
    throw std::runtime_error{"Unsupported VarType"};
  }

  switch (*type.value_as<NativeType>())
  {
    using enum NativeType;

    case Int:
      return int64_t{};
    case Decimal:
      return double{};
    case Bool:
      return bool{};
    case String:
      return std::string{};
    case Process:
      return ::Process{};
    case Pipeline:
      return ::Pipeline{};
    case ExecResult:
      return ::ExecResult{};
    default:
      throw std::runtime_error{"NativeType is invalid"};
      break;
  }
}


/// Represents a variable and its value.
/// If the variable can be a reference, Variable::val is empty and `val_ref`
/// is the actual value:
///
/// ```
/// fn foo(x: ref int)
/// {
///    x = 42;
/// }
///
/// a: int;
///
/// foo(ref a);
/// ```
///
/// For `x`, the Variable::val is empty, whilst Variable::val_ref
/// points to the Variable that represents `a`.
struct Variable
{
  Variable() = default;
  Variable(const VarValue& v) : val(v)
  {}

  VarValue& value() { return val; }
  const VarValue& value() const { return val; }

  Variable& get_ref_target() { return *val_ref; }
  Variable * get_ref() { return val_ref; }

  void set_ref_target(Variable * ref)
  {
    val = VarValue{};
    val_ref = ref;
  }

  bool is_ref() const { return val_ref != nullptr; }


private:
  VarValue val;
  Variable * val_ref{nullptr};
};


/// A stack entry is added to the stack on entry to each NodeBlock.
/// Each entry stores the variables in that stack, and a pointer to
/// the parent entry.
/// The stack entry is added before executing statements:
///
/// ```
/// fn foo()
/// {
///   a := "blah";
/// }
/// ```
/// - The entry for foo(), will store `a` as an empty string
/// - Execetion continues to the VariableAssign, setting `a` to "blah"
///
/// Also:
///
/// ```
/// a :int;
///
/// fn foo()
/// {
///   a :str;
/// }
/// ```
/// - The two `a` variables are entirely independent
/// - The `a:str` is in foo's stack entry, so a lookup for
///   `a` will resolve to `a:str`
class StackEntry
{
public:
  StackEntry(const ResolvedVariableMap& block_vars, StackEntry * parent)
    : m_parent(parent)
  {
    // initialise this NodeBlock's variables, assigning default values, which
    // can be replaced later when an VariableUse or VariableDecl is encountered
    for (const auto& [var, resolved] : block_vars) {
      m_vars.emplace(var, value_from_type(resolved.type));
    }
  }

  Variable& get_var(const VarName name)
  {
    return m_vars.at(name);
  }

  bool have_var (const VarName name) const
  {
    return m_vars.contains(name);
  }

  bool has_parent() const { return m_parent != nullptr; }
  StackEntry * get_parent() const { return m_parent; }
  std::map<VarName, Variable> get_vars() const { return m_vars; }

private:
  std::map<VarName, Variable> m_vars;
  StackEntry * m_parent{};
};

struct ExecutionResult
{
  ExecutionResult() = default;

  ExecutionResult(VarValue val)
    : value(std::move(val))
  {}

  VarValue value{std::monostate{}};
  bool returned{};
};

class Executor
{
public:
  Executor(const SymbolTable& sym_table, const RuntimeContext& rt_context);

  #ifdef CPY_UNIT_TESTS
    virtual ~Executor(){}
  #else
    ~Executor() = default;
  #endif

  ExecutionResult execute(const NodeBlock& block, std::map<VarName, Variable>&& func_args = {});

private:
  void execute (const NodeBlock& block, const VariableAssign& assign);
  void execute (const NodeBlock& block, const VariableDecl& decl);
  ExecutionResult execute (const NodeBlock& block, const Expression& expr);
  ExecutionResult execute (const NodeBlock& block, const ConditionIf& cond);
  ExecutionResult execute (const NodeBlock& block, const LoopWhile& loop);
  ExecutionResult execute (const NodeBlock& block, const FunctionCall& call);

  // operators //
  VarValue apply_operator(const NodeBlock& block, const Expression& lhs, const Expression& rhs, const BinaryOperator op);
  VarValue apply_operator(const NodeBlock& block, const Expression& operand, const UnaryOperator op);

  // variables //
  Variable& get_variable (const VarName name);

#ifdef CPY_UNIT_TESTS
  // this for the child class TestExecutor
  protected:
    virtual void push_stack (const NodeBlock& block, std::map<VarName, Variable> func_args = {});
    virtual void pop_stack ();

    std::stack<std::unique_ptr<StackEntry>> m_stack;
#else
  private:
    void push_stack (const NodeBlock& block, std::map<VarName, Variable> func_args = {});
    void pop_stack ();

    std::stack<std::unique_ptr<StackEntry>> m_stack;
#endif

private:
  const SymbolTable& m_sym_table;
  const RuntimeContext& m_rt_context;
};


#ifdef CPY_UNIT_TESTS

#include <algorithm>

using OnStackPush = std::function<void(std::map<VarName, Variable>, const size_t)>;
using OnStackPop = std::function<void(std::vector<std::map<VarName, Variable>>)>;

class TestExecutor : public Executor
{
public:
  TestExecutor(const SymbolTable& sym_table, OnStackPush on_push, OnStackPop on_pop)
    : Executor(sym_table, RuntimeContext{})
    , m_on_push(on_push)
    , m_on_pop(on_pop)
    {
    }

    TestExecutor(const SymbolTable& sym_table, OnStackPop on_pop)
      : Executor(sym_table, RuntimeContext{})
      , m_on_push()
      , m_on_pop(on_pop)
      {
      }

    void push_stack (const NodeBlock& block, std::map<VarName, Variable> func_args = {}) override
    {
      Executor::push_stack(block, std::move(func_args));
      if (m_on_push) {
        m_on_push(m_stack.top()->get_vars(), m_stack.size());
      }
    }

    void pop_stack () override
    {
      std::vector<std::map<VarName, Variable>> vars;
      vars.reserve(m_stack.size());

      StackEntry * entry = m_stack.top().get();

      while (entry)
      {
        vars.push_back(entry->get_vars());
        entry = entry->get_parent();
      }

      rg::reverse(vars);

      m_on_pop(std::move(vars));
      Executor::pop_stack();
    }

private:
  OnStackPush m_on_push;
  OnStackPop m_on_pop;
};


#endif
