#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <cpy/common.hpp>

enum class NodeType
{
  None,
  SyntaxError,
  SourceFile,
  FunctionDef,
  FunctionParam,
  FunctionBody,
  FunctionCall,
  Expression
};

enum class BuiltInType
{
  Int,
  Decimal,
  Bool,
  String,
  Void,
  Unknown
};


struct UserType
{
  std::string name; // TODO or std::string_view
};

struct VarType
{
  VarType(const BuiltInType t) : type (t)
  {
  }

  const auto& value() const { return type; }

  template<typename T>
  const std::optional<T> value_as() const
  {
    if (const auto param_type = std::get_if<T>(&type); param_type)
      return *param_type;
    return std::nullopt;
  }

  template<typename T>
  bool is_type() const requires(std::is_same_v<T, BuiltInType>)
  {
    return std::holds_alternative<T>(type);
  }

  bool is_type(const BuiltInType t) const
  {
    return is_type<BuiltInType>() && *(value_as<BuiltInType>()) == t;
  }

private:
  std::variant<BuiltInType, UserType> type;
};


inline bool operator==([[maybe_unused]] const UserType& a, [[maybe_unused]] const UserType& b)
{
  throw std::runtime_error{"Comparing unsupported UserType"};
}

inline bool operator==(const VarType& a, const VarType& b)
{
  return a.value() == b.value();
}

// AST nodes //

struct AstNode
{
  AstNode(const NodeType t) : type(t)
  {}

  SourceRegion source;

  NodeType node_type() const { return type; }
  bool is_node_type(const NodeType t) const { return t == node_type(); }

  virtual ~AstNode() = default;
  virtual void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const = 0;

private:
  NodeType type;
};

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
  FuncCall
};


inline std::string_view to_string(const BuiltInType t);
inline std::string_view to_string(const VarType& t);
inline std::string_view to_string(const BinaryOperator op);


struct Expression : public AstNode
{
  Expression(const ExpressionType t)
    : AstNode(NodeType::Expression)
    , ex_type(t)
  {}

  virtual ~Expression() = default;

  virtual bool is_expr_type(const ExpressionType t) const { return t == expr_type(); }
  virtual bool is_convertible_to([[maybe_unused]] const BuiltInType t) const { return false; }

  ExpressionType expr_type() const { return ex_type; }

private:
  ExpressionType ex_type;
};

struct IntegerLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Int;

  explicit IntegerLiteral(const int64_t val) : Expression(ExprType), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Int;
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

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    lhs->dump(os, tab);
    os << ' ' << to_string(op) << ' ';
    rhs->dump(os, tab);
  }

  std::unique_ptr<Expression> lhs;
  BinaryOperator op;
  std::unique_ptr<Expression> rhs;

};


template<typename ExprT> requires (std::derived_from<ExprT, Expression>)
const ExprT& get_expression(const std::unique_ptr<Expression>& expr)
{
  if (!expr->is_expr_type(ExprT::ExprType)) {
    throw std::runtime_error{"get_expression() called with expr and ExprT mismatch"};
  }
  return dynamic_cast<ExprT&>(*expr);
}

// Functions //

struct FunctionParam : public AstNode
{
  VarType type;
  std::string name;
  bool valid{true};

  FunctionParam() : FunctionParam("")
  {}

  FunctionParam (const std::string_view name) : FunctionParam(BuiltInType::Unknown, name, false)
  {}

  FunctionParam(const VarType type) : FunctionParam(type, "")
  {}

  FunctionParam(const VarType type, std::string_view name, const bool valid = true)
    : AstNode(NodeType::FunctionParam)
    , type(type)
    , name(name)
    , valid(valid)
  {}

public:

  template<typename T>
  bool is_type() const
  {
    return std::holds_alternative<T>(type);
  }

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << name << ":" << to_string(type) << '\n';
  }
};

template<bool CheckName>
struct FunctionParamComparer
{
  bool operator()(const FunctionParam& a, const FunctionParam& b) const
  {
    if constexpr (CheckName)
      return a.type == b.type && a.name == b.name;
    else
      return a.type == b.type;
  }
};

using FunctionParamCmp = FunctionParamComparer<true>;
using FunctionParamCmpIgnoreName = FunctionParamComparer<false>;

inline bool operator==(const FunctionParam& a, const FunctionParam& b)
{
  return FunctionParamCmp{}(a, b);
}


struct FunctionCall : public Expression
{
  std::string_view module;
  std::string_view name;
  std::vector<std::unique_ptr<Expression>> args;

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args = {})
    : Expression(ExpressionType::FuncCall),
      name(name)
    , args(std::move(args))
  {}

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args, const std::string_view module)
    : Expression(ExpressionType::FuncCall)
    , module(module)
    , name(name)
    , args(std::move(args))
  {}

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
};

struct FunctionBody : public AstNode
{
  FunctionBody() : AstNode(NodeType::FunctionBody)
  {}

  std::vector<std::unique_ptr<AstNode>> nodes;

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    for (const auto& n : nodes)
      n->dump(os);
  }
};

struct FunctionDef : public AstNode
{
  FunctionDef() : AstNode(NodeType::FunctionDef)
  {}

  std::string name;
  std::vector<FunctionParam> params;
  VarType return_type{BuiltInType::Void};
  FunctionBody body;

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << "() -> " << to_string(return_type) << '\n';

    for(const auto& p : params)
    {
      os << "  > " ;
      p.dump(os, tab);
    }

    body.dump(os, tab);
  }
};


struct SyntaxError : public AstNode
{
  SyntaxError() : AstNode(NodeType::SyntaxError)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "ERROR\n";
  }
};


struct SourceFile : public AstNode
{
  SourceFile() : AstNode(NodeType::SourceFile)
  {}

  std::vector<std::unique_ptr<AstNode>> nodes;
  fs::path src_path;

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << (src_path.empty() ? "" : src_path.string()) << '\n';

    for (const auto& n : nodes)
    {
      n->dump(os) ;
      os << '\n';
    }
  }
};


// to_string
inline std::string_view to_string(const BuiltInType t)
{
  switch (t)
  {
    using enum BuiltInType;

    case Int:
      return "int";
    case Decimal:
      return "dec";
    case Bool:
      return "bool";
    case String:
      return "str";
    case Void:
      return "void";
    default:
      return "Unknown";
  }
}

inline std::string_view to_string(const VarType& t)
{
  if (t.is_type<BuiltInType>()) {
    return to_string(std::get<BuiltInType>(t.value()));
  }
  else {
     throw std::runtime_error{"UserType not implemented"};
  }
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
