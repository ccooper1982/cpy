#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <cpy/ast/expressions.hpp>
#include <cpy/ast/ast_node.hpp>


struct NodeBlock : public AstNode
{
  static constexpr NodeType Type = NodeType::NodeBlock;

  using Nodes = std::vector<NodePtr<AstNode>>;


  NodeBlock() : AstNode(Type)
  {
    m_nodes.reserve(6);
  }

  void add_node(NodePtr<AstNode>&& node)
  {
    if (node) {
      m_nodes.push_back(std::move(node));
    }
  }

  void set_nodes(Nodes&& nodes)
  {
    m_nodes = std::move(nodes);
  }

  const Nodes& nodes() const { return m_nodes; }
  size_t node_count() const  { return m_nodes.size(); }


  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    for (const auto& n : m_nodes)
    {
      os << std::string(tab*2, ' ');
      n->dump(os, tab);
      os << '\n';
    }
  }

protected:
  Nodes m_nodes;
};


// Conditions //

struct ConditionIf : public AstNode
{
  struct ConditionBody
  {
    std::unique_ptr<Expression> condition;
    NodeBlock body;
  };

  ConditionIf() : AstNode(NodeType::ConditionIf)
  {}

  const NodeBlock& if_body () const { return if_.body; }
  size_t if_body_node_count() const { return if_body().nodes().size() ; }
  size_t else_if_count () const { return else_if.size(); }
  bool have_else() const { return else_body.has_value(); }

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    auto dump_condition = [&os, tab](const std::string_view type, const std::unique_ptr<Expression>& condition)
    {
      if (condition)
      {
        os << type << " (" ;
        condition->dump(os, tab);
        os << ") ";
      }
    };

    auto dump_body = [&os, tab](const std::string_view type, const NodeBlock& block)
    {
      os << type << "{\n";
      block.dump(os, tab+1);
      os << std::setw((tab+1)*2) << "}\n";
    };

    dump_condition("if", if_.condition);
    dump_body("", if_.body);

    for (const auto& cb : else_if)
    {
      dump_condition("else if", cb.condition);
      dump_body("", cb.body);
    }

    if (else_body) {
      dump_body("else", *else_body);
    }
  }

  ConditionBody if_;
  std::vector<ConditionBody> else_if;
  std::optional<NodeBlock> else_body;
};


// Functions Definition //

struct FunctionParam : public AstNode
{
  static constexpr NodeType Type = NodeType::FunctionParam;

  FunctionParam() : AstNode(NodeType::FunctionParam)
  {}

  FunctionParam(const std::string_view param_name, const std::string_view type)
    : AstNode(NodeType::FunctionParam)
    , param_name(param_name)
    , type_name(type)
  {}

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << param_name << ":" << type_name << '\n';
  }

  std::string_view param_name;
  std::string_view type_name;
  bool is_ref{};
};


struct FunctionDef : public AstNode
{
  static constexpr NodeType Type = NodeType::FunctionDef;

  using FunctionParams = std::vector<FunctionParam>;

  FunctionDef() : AstNode(Type)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << "() -> " << return_type << '\n';

    for(const auto& p : params)
    {
      os << "  > " ;
      p.dump(os, tab);
    }

    body.dump(os, tab);
  }

  void set_params(FunctionParams parameters)
  {
    params = std::move(parameters);
  }

  const auto& get_params () const
  {
    return params;
  }

  size_t param_count() const { return params.size(); }
  bool has_params() const { return !params.empty(); }

  std::string_view name;
  std::string_view return_type;
  NodeBlock body;

private:
  FunctionParams params;
};


//  Variables //

// <var_name>: <var_type>;                  // explicit type
// <var_name>: <var_type> = <init_expr>;    // explicit type, initialised
// <var_name> := <init_expr>;               // inferred type, initialised
// foo: int;
// foo: int = 5;
// foo := 5;
struct VariableDecl : public AstNode
{
  static constexpr NodeType Type = NodeType::VariableDecl;

  enum class DeclType
  {
    ExplicitNoInit,
    ExplicitInit,
    InferredType
  };

  VariableDecl() : AstNode(Type)
  {}

  static VariableDecl create_explicit(std::unique_ptr<Expression>&& init)
  {
    VariableDecl decl;
    decl.decl_type = init ? DeclType::ExplicitInit : DeclType::ExplicitNoInit;
    decl.initialiser = std::move(init);
    return decl;
  }

  static VariableDecl create_inferred(std::unique_ptr<Expression>&& init)
  {
    if (!init) {
      throw std::runtime_error{"VariableDecl - InferredType created without init expr"};
    }

    VariableDecl decl;
    decl.decl_type = DeclType::InferredType;
    decl.initialiser = std::move(init);
    return decl;
  }

  DeclType get_decl_type() const
  {
    return decl_type;
  }

  bool has_explicit_type() const
  {
    return decl_type == DeclType::ExplicitNoInit || decl_type == DeclType::ExplicitInit;
  }

  bool has_initialiser() const
  {
    return initialiser != nullptr;
  }

  bool is_literal_initialised() const
  {
    return has_initialiser() && initialiser->is_literal();
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << var_name << ':';

    if (has_explicit_type()) {
      os << var_type;
    }
    else if (initialiser) {
      os << "[inferred " << to_string(*initialiser) << "]";
    }
  }

  std::string_view var_name;
  std::string_view var_type;
  std::unique_ptr<Expression> initialiser;

  private:
    DeclType decl_type;
};


struct VariableAssign : public AstNode
{
  static constexpr NodeType Type = NodeType::VariableAssign;

  VariableAssign() : AstNode(Type)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << ':';
    expr->dump(os, tab);
  }

  std::string_view name;
  NodePtr<Expression> expr;
};


struct SyntaxError : public AstNode
{
  static constexpr NodeType Type = NodeType::SyntaxError;

  SyntaxError() : AstNode(Type)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "ERROR\n";
  }
};


struct SourceFile : public AstNode
{
  static constexpr NodeType Type = NodeType::SourceFile;

  SourceFile() : AstNode(Type)
  {}

  NodeBlock block;
  fs::path src_path;

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << (src_path.empty() ? "" : src_path.string()) << '\n';
    block.dump(os, tab);
  }
};
