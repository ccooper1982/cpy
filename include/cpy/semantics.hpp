#pragma once

#include <concepts>
#include <functional>
#include <set>

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/ast/ast_node.hpp>


class Semantics
{
public:
  void process(Script& script);

private:
  template<typename NodeT>
  using NodeHandler = std::function<void(const NodeT&)>;

  void create_cache(const SourceFile& root);
  void process_function_call(const SourceFile& root, const FunctionCall& call, Issues& issues);


  template<typename NodeT, typename Handler>
    requires std::derived_from<NodeT, AstNode> &&
             std::convertible_to<decltype(NodeT::Type), NodeType>
  void walk_ast(const SourceFile& root, Handler&& handler)
  {
    const NodeType nt = NodeT::Type;

    for (const auto& node : root.nodes | vw::filter([&](const std::unique_ptr<AstNode>& n) { return n->is_node_type(nt);})) {
      handler(dynamic_cast<const NodeT&>(*node));
    }
  }

private:
  std::set<std::string_view> m_function_names;
};
