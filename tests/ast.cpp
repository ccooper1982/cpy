#include <gtest/gtest.h>

#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <variant>

TEST(Parser, ZeroNodes)
{
  Parser parser;
  const auto script = parser.parse("");

  ASSERT_EQ(script.ast->nodes.size(), 0);
}

TEST(Parser, FuncCall_NotExist)
{
  const std::string_view src = R"(
    hello();
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionCall);
  ASSERT_EQ(dynamic_cast<FunctionCall&>(*script.ast->nodes[0]).name, "hello");
}

TEST(Parser, FuncDef_NoArgs)
{
  const std::string_view src = R"(
    fn hello() {}
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_TRUE(def.params.empty());
  ASSERT_EQ(def.name, "hello");
}


TEST(Parser, FuncDef_1Arg)
{
  const std::string_view src = R"(
    fn hello(a: int) {}
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);

  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 1);
  ASSERT_EQ(def.params[0].name, "a");
  ASSERT_TRUE(std::get_if<BuiltInType>(&(def.params[0].type.value())) != nullptr);
  ASSERT_TRUE(def.params[0].type.value_as<BuiltInType>().has_value());
  ASSERT_EQ(def.params[0].type.value_as<BuiltInType>(), std::optional<BuiltInType>{BuiltInType::Int});
}

TEST(Parser, FuncDef_2Arg)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str) {}
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);

  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 2);
  ASSERT_EQ(def.params[0].name, "a");

  const FunctionParam& p1 = def.params[0];
  ASSERT_EQ(p1.name, "a");
  ASSERT_TRUE(std::get_if<BuiltInType>(&(p1.type.value())) != nullptr);
  ASSERT_TRUE(p1.type.value_as<BuiltInType>().has_value());
  ASSERT_EQ(p1.type.value_as<BuiltInType>(), std::optional<BuiltInType>{BuiltInType::Int});

  const FunctionParam& p2 = def.params[1];
  ASSERT_EQ(p2.name, "b");
  ASSERT_TRUE(std::get_if<BuiltInType>(&(p2.type.value())) != nullptr);
  ASSERT_TRUE(p2.type.value_as<BuiltInType>().has_value());
  ASSERT_EQ(p2.type.value_as<BuiltInType>(), std::optional<BuiltInType>{BuiltInType::String});
}

TEST(Parser, FuncDef_1Arg_InvalidType)
{
  const std::string_view src = R"(
    fn hello(a: foo) {}
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);

  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 1);
  ASSERT_EQ(def.params[0].name, "a");

  const FunctionParam& p1 = def.params[0];
  ASSERT_FALSE(p1.valid);
}

TEST(Parser, SyntaxError)
{
  const std::string_view src = R"(
    fn hello() {
  )";

  Parser parser;
  const auto script = parser.parse(src);
}


TEST(Parser, Blah)
{
  // auto by_node_type = [](const NodeType nt)
  // {
  //   return [nt](const std::unique_ptr<AstNode>& n){ return n->is_node_type(nt); };
  // };

  // for (const auto& func_call_node : script.ast->nodes | vw::filter(by_node_type(NodeType::FunctionCall))) {
  //   ASSERT_EQ(dynamic_cast<FunctionDef&>(*func_call_node).name, "hello");
  // }
}
