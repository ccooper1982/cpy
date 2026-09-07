#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <cpy/common.hpp>

enum class NodeType
{
  None,
  Error,
  SourceFile,
  Function,
  FunctionParam
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
  // if (a.type.index() != b.type.index())
  //   return false;

  // if (a.type.index() == 0) {
  //   return std::get<0>(a.type) == std::get<0>(b.type);
  // }
  // else {
  //   throw std::runtime_error{"Comparing unsupported UserType"};
  // }
}

struct AstNode
{
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

struct Function : public AstNode
{
  std::string name;
  std::vector<FunctionParam> params;
  VarType return_type{BuiltInType::Void};

  // FunctionBody body;
  // std::vector<std::unique_ptr<AstNode>> nodes;

  NodeType node_type() const override { return NodeType::Function; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << " -> " << return_type.to_string() << ':' << '\n';

    for(const auto& p : params)
      p.dump(os, tab);
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
    os << (src_path.empty() ? "Compiled source" : src_path.string()) << '\n';

    for (const auto& n : nodes)
      n->dump(os);
  }
};
