#pragma once

#include <cstdint>
#include <ios>
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

  std::string_view to_string() const
  {
    if (is_type<BuiltInType>())
    {
      switch (const auto t = std::get<BuiltInType>(type) ; t)
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
    else {
      throw std::runtime_error{"UserType not implemented"};
    }
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


// Expressions
enum class ExpressionType
{
  Int,
  String,
  Dec,
  Bool
};

struct Expression : public AstNode
{
  Expression(const ExpressionType t)
    : AstNode(NodeType::Expression)
    , expr_type(t)
  {}

  virtual ~Expression() = default;

  virtual bool is_expr_type(const ExpressionType t) const { return t == expr_type; };

  ExpressionType expr_type;
};

struct IntegerLiteral : public Expression
{
  explicit IntegerLiteral(const int64_t val) : Expression(ExpressionType::Int), v(val) {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "int = " << v;
  }

  int64_t v;
};

struct StringLiteral : public Expression
{
  explicit StringLiteral(const std::string_view val) : Expression(ExpressionType::String), v(val) {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "str = " << v;
  }

  std::string_view v;
};

struct DecimalLiteral : public Expression
{
  explicit DecimalLiteral(const double val) : Expression(ExpressionType::Dec), v(val) {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "dec = " << v;
  }

  double v;
};

struct BooleanLiteral : public Expression
{
  explicit BooleanLiteral(const bool val) : Expression(ExpressionType::Bool), v(val) {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "bool = " << std::boolalpha << v;
  }

  bool v;
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

struct FunctionParam : public AstNode
{
  VarType type;
  std::string name;
  bool valid{true};

  FunctionParam() : FunctionParam("")
  {

  }

  FunctionParam (const std::string_view name) : FunctionParam(BuiltInType::Unknown, name, false)
  {

  }

  FunctionParam(const VarType type) : FunctionParam(type, "")
  {
  }

  FunctionParam(const VarType type, std::string_view name, const bool valid = true)
    : AstNode(NodeType::FunctionParam)
    , type(type)
    , name(name)
    , valid(valid)
  {
  }

public:

  template<typename T>
  bool is_type() const
  {
    return std::holds_alternative<T>(type);
  }

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << name << ":" << type.to_string() << '\n';
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


struct FunctionCall : public AstNode
{
  std::string_view module;
  std::string_view name;
  std::vector<std::unique_ptr<Expression>> args;

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args = {})
    : AstNode(NodeType::FunctionCall),
      name(name)
    , args(std::move(args))
  {
  }

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args, const std::string_view module)
    : AstNode(NodeType::FunctionCall)
    , module(module)
    , name(name)
    , args(std::move(args))
  {
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
    os << ')' << '\n';
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
    os << name << ": \n -> " << return_type.to_string() << '\n';

    for(const auto& p : params) {
      os << "  > " ;
      p.dump(os, tab);
    }

    body.dump(os, tab);
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
      n->dump(os);
  }
};


// useful
inline bool param_arg_valid(const FunctionParam& def_param, const std::unique_ptr<Expression>& call_arg)
{
  if (const auto param_type = def_param.type.value_as<BuiltInType>() ; !param_type)
    throw std::runtime_error("Function has unsupported UserType parameter");
  else
  {
    switch (*param_type)
    {
      using enum BuiltInType;
      case Int:
        return call_arg->is_expr_type(ExpressionType::Int);

      case String:
        return call_arg->is_expr_type(ExpressionType::String);

      case Decimal:
        return call_arg->is_expr_type(ExpressionType::Dec);

      case Bool:
        return call_arg->is_expr_type(ExpressionType::Bool);

      case Unknown:
        return false;

      default:
        throw std::runtime_error("Function has unsupported BuiltInType parameter");
        break;
    }
    return false; // appease clang
  }
}

inline bool func_call_valid(const FunctionDef& def, const FunctionCall& call)
{
  if (def.name != call.name) {
    return false;
  }

  return std::ranges::equal(def.params, call.args, [](const auto& param, const auto& arg) {
      return param_arg_valid(param, arg);
    }
  );
}
