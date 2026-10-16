#pragma once

#include <cpy/ast/ast_node.hpp>
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
  VariableRef
};


struct AstNode;

struct Expression : public AstNode
{
  static constexpr NodeType Type = NodeType::Expression;

  Expression(const ExpressionType t)
    : AstNode(Type)
    , ex_type(t)
  {}

  virtual ~Expression() = default;

  virtual bool is_expr_type(const ExpressionType t) const { return t == expr_type(); }
  virtual bool is_convertible_to([[maybe_unused]] const BuiltInType t) const { return false; }
  virtual VarType get_var_type() const = 0;

  ExpressionType expr_type() const { return ex_type; }

private:
  ExpressionType ex_type;
};



inline std::string_view to_string(const Expression& expr);
inline std::string_view to_string(const BinaryOperator op);


struct IntegerLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Int;

  explicit IntegerLiteral(const int64_t val) : Expression(ExprType), v(val)
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

  int64_t v;
};

struct StringLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::String;

  explicit StringLiteral(const std::string_view val) : Expression(ExprType), v(val)
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
    os << "str = " << v;
  }

  std::string_view v;
};

struct DecimalLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Dec;

  explicit DecimalLiteral(const double val) : Expression(ExprType), v(val)
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

  double v;
};

struct BooleanLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Bool;

  explicit BooleanLiteral(const bool val) : Expression(ExprType), v(val)
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

  bool v;
};


struct BinaryExpression : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Binary;

  explicit BinaryExpression() : Expression(ExprType)
  {}

  explicit BinaryExpression(std::unique_ptr<Expression>&& lhs, std::unique_ptr<Expression>&& rhs, const BinaryOperator op)
  : Expression(ExprType)
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


struct FunctionCall : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::FuncCall;

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args = {})
    : Expression(ExprType)
    , name(name)
    , args(std::move(args))
  {}

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args, const std::string_view module)
    : Expression(ExprType)
    , module(module)
    , name(name)
    , args(std::move(args))
  {}

  VarType get_var_type() const override
  {
    // never called because the type is taken from the return type of
    // the function being called
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
      args[i]->dump(os, tab);
      if (i+1 < args.size())
        os << ',';
    }
    os << ')';
  }

  std::string_view module;
  std::string_view name;
  std::vector<NodePtr<Expression>> args;
};


// Note: not a "ref" like a C++ reference, but "a node which refers to a variable"
struct VariableRef : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::VariableRef;

  VariableRef() : Expression(ExprType)
  {}

  VariableRef(const std::string_view name)
    : Expression(ExprType)
    , name(name)
  {}

  VarType get_var_type() const override
  {
    // never called because the type is taken from the type of the variable
    throw std::runtime_error{"get_var_type() called on VariableRef"};
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
  return dynamic_cast<ExprT&>(*expr);
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
    case VariableRef:
      return "VariableReference";
  }

  std::unreachable();
}
