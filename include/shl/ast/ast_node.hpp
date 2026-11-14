#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <shl/common.hpp>

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
  ConditionIf,
  LoopWhile
};

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
