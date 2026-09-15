#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
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
  Float,
  Bool,
  String,
  Void,
};

struct UserType
{
  std::string name; // TODO or std::string_view
};

struct VarType
{
  std::variant<BuiltInType, UserType> type;

  VarType(const BuiltInType t) : type (t) {}

  std::string_view to_string() const
  {
    if (std::holds_alternative<BuiltInType>(type)) {
      switch (const auto t = std::get<BuiltInType>(type) ; t) {
        using enum BuiltInType;

        case Int:
          return "int";
        case Float:
          return "float";
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
};

inline bool operator==(const UserType& a, const UserType& b)
{
  throw std::runtime_error{"Comparing unsupported UserType"};
  // return a.name == b.name;
}

inline bool operator==(const VarType& a, const VarType& b)
{
  return a.type == b.type;
}

// Expressions
struct IntegerLiteral
{
  IntegerLiteral(const uint64_t i) : i(i) {}
  uint64_t i;
};
struct StringLiteral
{
  StringLiteral(const std::string_view s) : s(s) {}
  std::string_view s;
};

using Expression = std::variant<IntegerLiteral, StringLiteral>;


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
  // helper type for the visitor
  template<class... Ts>
  struct overloads : Ts... { using Ts::operator()...; };

  Expression value;

  FunctionArg (const StringLiteral& v) : value(v)
  {
  }

  FunctionArg (const IntegerLiteral& v) : value(v)
  {
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const
  {
    const auto visitor = overloads
    {
        [&](const IntegerLiteral& v){ os << "int = " << v.i; },
        [&](const StringLiteral& v){ os << "str = " << v.s; }
    };

    std::visit(visitor, value);
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
      // os << args[i].value;
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
