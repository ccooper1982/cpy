#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
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
  std::string type;
  std::string name;

  FunctionParam(const std::string_view type, std::string_view name) : type(type), name(name)
  {

  }


  NodeType node_type() const override { return NodeType::FunctionParam; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << name << ":" << type << '\n';
  }
};

struct Function : public AstNode
{
  std::string name;
  std::vector<FunctionParam> params;
  std::string return_type{"void"};

  // FunctionBody body;
  // std::vector<std::unique_ptr<AstNode>> nodes;

  NodeType node_type() const override { return NodeType::Function; }
  bool is_node_type(const NodeType t) const override { return node_type() == t; }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << " -> " << return_type << ':' << '\n';

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
