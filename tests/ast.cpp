#include <gtest/gtest.h>

#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <string_view>


TEST(Ast, ZeroNodes)
{
  Parser parser;
  const auto script = parser.parse(std::string_view{});

  ASSERT_EQ(script.ast->nodes.size(), 0);
}

TEST(Ast, FuncCall_NotExist)
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

TEST(Ast, FuncDef_NoArgs)
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


TEST(Ast, FuncDef_1Arg)
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
  ASSERT_TRUE(def.params[0].type.is_type<BuiltInType>());
  ASSERT_TRUE(def.params[0].type.is_type(BuiltInType::Int));
}

TEST(Ast, FuncDef_2Arg)
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
  ASSERT_TRUE(p1.type.is_type<BuiltInType>());
  ASSERT_TRUE(p1.type.is_type(BuiltInType::Int));

  const FunctionParam& p2 = def.params[1];
  ASSERT_EQ(p2.name, "b");
  ASSERT_TRUE(p2.type.is_type(BuiltInType::String));
}

TEST(Ast, FuncDef_1Arg_InvalidType)
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

TEST(Ast, FuncCall_NoArgs)
{
  const std::string_view src = R"(
    fn hello() {}
    hello();
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::FunctionCall);

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call.name, "hello");
  ASSERT_TRUE(call.args.empty());
}

TEST(Ast, FuncCall_Args)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str) {}
    hello(1, "2");
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::FunctionCall);

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call.name, "hello");
  ASSERT_EQ(call.args.size(), 2);
  ASSERT_TRUE(call.args[0]->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(call.args[1]->is_expr_type(ExpressionType::String));

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_TRUE(param_arg_valid(def.params[0], call.args[0]));
  ASSERT_TRUE(param_arg_valid(def.params[1], call.args[1]));
  ASSERT_FALSE(param_arg_valid(def.params[0], call.args[1]));
  ASSERT_FALSE(param_arg_valid(def.params[1], call.args[0]));

  ASSERT_TRUE(func_call_valid(def, call));
}

TEST(Ast, FuncCall_InvalidCall)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str) {}
    hello(1, 2);
    hello(1, "2", 3);
    hello(1);
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 4);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::FunctionCall);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  const auto& wrong_type = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  const auto& too_many = dynamic_cast<FunctionCall&>(*script.ast->nodes[2]);
  const auto& too_few = dynamic_cast<FunctionCall&>(*script.ast->nodes[3]);

  ASSERT_FALSE(func_call_valid(def, wrong_type));
  ASSERT_FALSE(func_call_valid(def, too_many));
  ASSERT_FALSE(func_call_valid(def, too_few));
}

TEST(Ast, FuncCall_AllPrimitives)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str, c: dec, d: bool) {}

    hello(1, "one", 1.1, true);
  )";

  Parser parser;
  const auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::FunctionCall);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_EQ(def.params.size(), 4);

  const auto& params = def.params;
  ASSERT_TRUE(params[0].type.is_type(BuiltInType::Int));
  ASSERT_TRUE(params[1].type.is_type(BuiltInType::String));
  ASSERT_TRUE(params[2].type.is_type(BuiltInType::Decimal));
  ASSERT_TRUE(params[3].type.is_type(BuiltInType::Bool));

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call.args.size(), 4);
  ASSERT_TRUE(call.args[0]->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(call.args[1]->is_expr_type(ExpressionType::String));
  ASSERT_TRUE(call.args[2]->is_expr_type(ExpressionType::Dec));
  ASSERT_TRUE(call.args[3]->is_expr_type(ExpressionType::Bool));
}

TEST(Ast, SyntaxError)
{
  {
    Parser parser;
    const std::string_view src = R"(
      fn hello(a: int, b: str) {
    )";

    const auto script = parser.parse(src);

    ASSERT_EQ(script.ast->nodes.size(), 1);
    ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  }

  {
    Parser parser;
    const std::string_view src = R"(
      fn hello(a: int, b: ) {}
    )";

    const auto script = parser.parse(src);

    ASSERT_EQ(script.ast->nodes.size(), 1);
    ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

    const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
    ASSERT_EQ(def.name, "hello");

    ASSERT_EQ(def.params.size(), 2);
    ASSERT_EQ(def.params[0].name, "a");
    ASSERT_TRUE(def.params[0].type.is_type(BuiltInType::Int));

    ASSERT_EQ(def.params[1].name, "b");
    ASSERT_TRUE(def.params[1].type.is_type(BuiltInType::Unknown));
  }
}
