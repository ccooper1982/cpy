#pragma once

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
  Error,
  SourceFile,
  FunctionDef,
  FunctionParam,
  FunctionBody,
  FunctionCall
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

  std::string_view to_string() const
  {
    if (std::holds_alternative<BuiltInType>(type))
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

// Expressions
struct IntegerLiteral
{
  IntegerLiteral(const int64_t val) : v(val) {}
  int64_t v;
};
struct StringLiteral
{
  StringLiteral(const std::string_view val) : v(val) {}
  std::string_view v;
};
struct DecimalLiteral
{
  explicit DecimalLiteral(const double val) : v(val) {}
  double v;
};

using Expression = std::variant<IntegerLiteral, StringLiteral, DecimalLiteral>;


// AST nodes

struct AstNode
{
  SourceRegion source;

  virtual NodeType node_type() const = 0;
  virtual bool is_node_type(const NodeType t) const = 0;

  virtual ~AstNode() = default;
  virtual void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const = 0;
};

struct Error : public AstNode
{
  NodeType node_type() const override { return NodeType::Error; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

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

  FunctionParam() : type(BuiltInType::Unknown), valid(false)
  {

  }

  FunctionParam (std::string_view name) : type(BuiltInType::Unknown), name(name), valid(false)
  {

  }

  FunctionParam(const VarType type) : type(type)
  {
  }

  FunctionParam(const VarType type, std::string_view name) : type(type), name(name)
  {
  }


  NodeType node_type() const override { return NodeType::FunctionParam; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << name << ":" << type.to_string() << '\n';
  }
};

template<bool CheckName = true>
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

struct FunctionArg
{
  Expression value;

  FunctionArg (const StringLiteral& v) : value(v)
  {}

  FunctionArg (const IntegerLiteral& v) : value(v)
  {}

  FunctionArg (const DecimalLiteral& v) : value(v)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const
  {
    const auto visitor = overloads
    {
      [&](const IntegerLiteral& v){ os << "int = " << v.v; },
      [&](const StringLiteral& v){ os << "str = " << v.v; },
      [&](const DecimalLiteral& v){ os << "dec = " << v.v; }
    };

    std::visit(visitor, value);
  }

  template<typename T>
  bool is_type() const
  {
    return std::holds_alternative<T>(value);
  }
};


struct FunctionCall : public AstNode
{
  std::string_view name;
  std::vector<FunctionArg> args;

  FunctionCall(const std::string_view name, std::vector<FunctionArg> args = {})
    : name(name)
    , args(std::move(args))
  {
  }

  NodeType node_type() const override { return NodeType::FunctionCall; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << '(';
    for (std::size_t i = 0; i < args.size() ; ++i)
    {
      args[i].dump(os, tab);
      if (i+1 < args.size())
        os << ',';
    }
    os << ')' << '\n';
  }
};

struct FunctionBody : public AstNode
{
  std::vector<std::unique_ptr<AstNode>> nodes;

  NodeType node_type() const override { return NodeType::FunctionBody; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    for (const auto& n : nodes)
      n->dump(os);
  }
};

struct FunctionDef : public AstNode
{
  std::string name;
  std::vector<FunctionParam> params;
  VarType return_type{BuiltInType::Void};
  FunctionBody body;

  NodeType node_type() const override { return NodeType::FunctionDef; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

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
  std::vector<std::unique_ptr<AstNode>> nodes;
  fs::path src_path;

  NodeType node_type() const override { return NodeType::SourceFile; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << (src_path.empty() ? "" : src_path.string()) << '\n';

    for (const auto& n : nodes)
      n->dump(os);
  }
};


// useful
inline bool param_arg_valid(const FunctionParam& def_param, const FunctionArg& call_arg)
{
  if (const auto param_type = def_param.type.value_as<BuiltInType>() ; !param_type)
    throw std::runtime_error("Function has unsupported UserType parameter");
  else
  {
    switch (*param_type)
    {
      using enum BuiltInType;
      case Int:
        return call_arg.is_type<IntegerLiteral>();

      case String:
        return call_arg.is_type<StringLiteral>();

      case Decimal:
        return call_arg.is_type<DecimalLiteral>();

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
  if (def.params.size() != call.args.size() || def.name != call.name) {
    std::cout << "name mismatch\n";
    return false;
  }

  size_t i{};
  for (const auto& param : def.params)
  {
    if (!param_arg_valid(param, call.args[i++]))
      return false;
  }
  return true;
}
