#pragma once

#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <memory>
#include <vector>

// Expressions //
enum class BinaryOperator
{
  Add,
  Subtract,
  Multiply,
  Divide,
  Equal,
  NotEqual,
  Less,
  Greater,
  LessEqual,
  GreaterEqual,
};

enum class ExpressionType
{
  Int,
  String,
  Dec,
  Bool,
  Binary,
  FuncCall,
  VariableUse,
  Return
};


struct AstNode;

struct Expression : public AstNode
{
  static constexpr NodeType Type = NodeType::Expression;

  Expression(const ExpressionType t, const bool is_literal)
    : AstNode(Type)
    , m_ex_type(t)
    , m_is_literal(is_literal)
  {}

  virtual ~Expression() = default;

  bool is_expr_type(const ExpressionType t) const { return t == expr_type(); }
  bool is_literal () const { return m_is_literal; };

  virtual bool is_convertible_to([[maybe_unused]] const BuiltInType t) const { return false; }
  virtual VarType get_var_type() const = 0;

  ExpressionType expr_type() const { return m_ex_type; }

private:
  ExpressionType m_ex_type;
  bool m_is_literal{};
};



inline std::string_view to_string(const Expression& expr);
inline std::string_view to_string(const BinaryOperator op);


struct Return : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Return;

  explicit Return() : Expression(ExprType, false)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "return ";
    if (return_expr) {
      return_expr->dump(os, tab);
    }
  }

  VarType get_var_type() const override
  {
    // if return_expr is a FunctionCall, the type is taken from the function's definition,
    // FunctionCall::get_var_type() will throw if called. Allow this path so it highlights
    // errors
    return return_expr ? return_expr->get_var_type() : BuiltInType::Void;
  }

  bool has_expression() const
  {
    return return_expr != nullptr;
  }

  NodePtr<Expression> return_expr;
};


struct IntegerLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Int;

  explicit IntegerLiteral(const CpyInt val) : Expression(ExprType, true), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Int;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Int;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "int = " << v;
  }

  int64_t value() const
  {
    return v;
  }

  CpyInt v;
};

struct StringLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::String;

  explicit StringLiteral(const std::string_view val) : Expression(ExprType, true), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::String;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::String;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "str = \"" << v << '\"';
  }

  CpyString value() const
  {
    return CpyString{v};
  }

  std::string_view v;
};

struct DecimalLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Dec;

  explicit DecimalLiteral(const double val) : Expression(ExprType, true), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Decimal;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Decimal;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "dec = " << v;
  }

  CpyDec value() const
  {
    return v;
  }

  CpyDec v;
};

struct BooleanLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Bool;

  explicit BooleanLiteral(const bool val) : Expression(ExprType, true), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Bool;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Bool;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "bool = " << std::boolalpha << v;
  }

  CpyBool value() const
  {
    return v;
  }

  CpyBool v;
};


struct BinaryExpression : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Binary;

  explicit BinaryExpression() : Expression(ExprType, false)
  {}

  explicit BinaryExpression(std::unique_ptr<Expression>&& lhs, std::unique_ptr<Expression>&& rhs, const BinaryOperator op)
  : Expression(ExprType, false)
  , lhs(std::move(lhs))
  , op(op)
  , rhs(std::move(rhs))
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return lhs->is_convertible_to(t) && rhs->is_convertible_to(t);
  }

  VarType get_var_type() const override
  {
    if (lhs->is_expr_type(ExpressionType::FuncCall) || rhs->is_expr_type(ExpressionType::FuncCall)) {
      throw std::runtime_error{"get_var_type() called on BinaryExpression with FunctionCall operand(s)"};
    }

    if (lhs->get_var_type() == rhs->get_var_type()) {
      return lhs->get_var_type();
    }
    else {
      throw std::runtime_error{"get_var_type() called on BinaryExpression with incompatible operands"};
    }
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    lhs->dump(os, tab);
    os << ' ' << to_string(op) << ' ';
    rhs->dump(os, tab);
  }

  NodePtr<Expression> lhs;
  BinaryOperator op;
  NodePtr<Expression> rhs;

};

// Function Call //
struct FunctionArgument
{
  NodePtr<Expression> expr;
  bool is_ref{};
};


struct FunctionCall : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::FuncCall;

  FunctionCall(const std::string_view name, std::vector<FunctionArgument> args = {})
    : Expression(ExprType, false)
    , name(name)
    , args(std::move(args))
  {}

  FunctionCall(const std::string_view name, std::vector<FunctionArgument> args, const std::string_view module)
    : Expression(ExprType, false)
    , module(module)
    , name(name)
    , args(std::move(args))
  {}

  VarType get_var_type() const override
  {
    // type is the return type of the function definition, not the call site
    throw std::runtime_error{"get_var_type() called on FunctionCall"};
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    if (!module.empty()) {
      os << module << "::";
    }
    os << name << '(';
    for (std::size_t i = 0; i < args.size() ; ++i)
    {
      args[i].expr->dump(os, tab);
      if (i+1 < args.size())
        os << ',';
    }
    os << ')';
  }

  std::string_view module;
  std::string_view name;
  std::vector<FunctionArgument> args;
};



// The use of a variable, i.e.:
//  ```
//  a: int;
//  foo(a);
// ```
// the `a` is a VariableUse
struct VariableUse : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::VariableUse;

  VariableUse() : Expression(ExprType, false)
  {}

  VariableUse(const std::string_view name)
    : Expression(ExprType, false)
    , name(name)
  {}

  VarType get_var_type() const override
  {
    // never called because the type is taken from the type of the variable
    throw std::runtime_error{"get_var_type() called on VariableUse"};
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name;
  }

  std::string_view name;
};


inline std::string_view to_string(const Expression& expr);


template<typename ExprT> requires (std::derived_from<ExprT, Expression>)
const ExprT& get_expression(const std::unique_ptr<Expression>& expr)
{
  if (!expr->is_expr_type(ExprT::ExprType)) {
    throw std::runtime_error{"get_expression() called with expr and ExprT mismatch"};
  }
  return static_cast<ExprT&>(*expr);
}


inline std::string_view to_string(const BinaryOperator op)
{
  switch (op)
  {
    case BinaryOperator::Add:         return "+";
    case BinaryOperator::Subtract:    return "-";
    case BinaryOperator::Multiply:    return "*";
    case BinaryOperator::Divide:      return "/";
    case BinaryOperator::Equal:       return "==";
    case BinaryOperator::NotEqual:    return "!=";
    case BinaryOperator::Less:        return "<";
    case BinaryOperator::Greater:     return ">";
    case BinaryOperator::LessEqual:   return "<=";
    case BinaryOperator::GreaterEqual: return ">=";
  }

  std::unreachable();
}

inline std::string_view to_string(const Expression& expr)
{
  switch (expr.expr_type())
  {
    using enum ExpressionType;

    case Dec:
      return "DecimalLiteral";
    case Int:
      return "IntegerLiteral";
    case Bool:
      return "BooleanLiteral";
    case String:
      return "StringLiteral";
    case Binary:
      return "BinaryExpression";
    case FuncCall:
      return "FunctionCall";
    case VariableUse:
      return "VariableReference";
    case Return:
      return "Return";
  }

  std::unreachable();
}
