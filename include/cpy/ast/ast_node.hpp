#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <cpy/common.hpp>

enum class NodeType
{
  None,
  SyntaxError,
  SourceFile,
  FunctionDef,
  FunctionParam,
  NodeBlock,
  Expression,
  VariableDecl,
  VariableAssign,
  ConditionIf
};

enum class BuiltInType
{
  Unset,
  Int,
  Decimal,
  Bool,
  String,
  Void
};

inline constexpr const auto unresolved_t = BuiltInType::Unset;


struct UserType
{
  std::string_view name;
};

struct VarType
{
  VarType() : type (BuiltInType::Unset)
  {}

  VarType(const BuiltInType t) : type (t)
  {}

  const auto& value() const
  {
    return type;
  }

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

  explicit operator bool() const
  {
    return !is_type(BuiltInType::Unset);
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


struct AstNode
{
  AstNode(const NodeType t) : type(t)
  {}

  NodeType node_type() const { return type; }
  bool is_node_type(const NodeType t) const { return t == node_type(); }
  bool is_expression() const { return is_node_type(NodeType::Expression); }

  virtual ~AstNode() = default;
  virtual void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const = 0;

  SourceRegion source;

private:
  NodeType type;
};

template<typename NodeT>
using NodePtr = std::unique_ptr<NodeT>;
