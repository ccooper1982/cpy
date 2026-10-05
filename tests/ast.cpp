#include <gtest/gtest.h>

#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <string_view>

/// The AST tests confirm the AST contains expected node types.
/// Syntax errors are found during creating the AST, though has not been a priority.
/// The semantic checker is where most of the checks are:
/// - AST : what the user wrote
/// - Semamtics : what the user meant
/// i.e. the AST does not consider variable types, that is handled by Semantics.

TEST(Ast, ZeroNodes)
{
  Parser parser;
  auto script = parser.parse(std::string_view{});
  ASSERT_EQ(script.ast->nodes.size(), 0);
}

// Function Definition //

TEST(Ast, FuncDef_NoArgs)
{
  const std::string_view src = R"(
    fn hello() {}
  )";

  Parser parser;
  auto script = parser.parse(src);

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
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);

  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 1);
  ASSERT_EQ(def.params[0].param_name, "a");
  ASSERT_EQ(def.params[0].type_name, "int");
}

TEST(Ast, FuncDef_2Arg)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str) {}
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);

  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 2);
  ASSERT_EQ(def.params[0].param_name, "a");

  const FunctionParam& p1 = def.params[0];
  ASSERT_EQ(p1.param_name, "a");
  ASSERT_EQ(p1.type_name, "int");

  const FunctionParam& p2 = def.params[1];
  ASSERT_EQ(p2.param_name, "b");
  ASSERT_EQ(p2.type_name, "str");
}

TEST(Ast, FuncDef_1Arg_InvalidType)
{
  const std::string_view src = R"(
    fn hello(a: foo) {}
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_EQ(def.name, "hello");
  ASSERT_EQ(def.params.size(), 1);
  ASSERT_EQ(def.params[0].param_name, "a");
}

// Function Call //
TEST(Ast, FuncCall_NotExist)
{
  const std::string_view src = R"(
    hello();
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 1);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::Expression);
  ASSERT_EQ(dynamic_cast<FunctionCall&>(*script.ast->nodes[0]).name, "hello");
}

TEST(Ast, FuncCall_NoArgs)
{
  const std::string_view src = R"(
    fn hello() {}
    hello();
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);

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
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call.name, "hello");
  ASSERT_EQ(call.args.size(), 2);
  ASSERT_TRUE(call.args[0]->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(call.args[1]->is_expr_type(ExpressionType::String));
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
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 4);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);
}

TEST(Ast, FuncCall_AllPrimitives)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str, c: dec, d: bool) {}

    hello(1, "one", 1.1, true);
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);

  const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_EQ(def.params.size(), 4);

  const auto& params = def.params;
  ASSERT_EQ(params[0].type_name, "int");
  ASSERT_EQ(params[1].type_name, "str");
  ASSERT_EQ(params[2].type_name, "dec");
  ASSERT_EQ(params[3].type_name, "bool");

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call.args.size(), 4);
  ASSERT_TRUE(call.args[0]->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(call.args[1]->is_expr_type(ExpressionType::String));
  ASSERT_TRUE(call.args[2]->is_expr_type(ExpressionType::Dec));
  ASSERT_TRUE(call.args[3]->is_expr_type(ExpressionType::Bool));
}

TEST(Ast, FuncCall_Module)
{
  const std::string_view src = R"(
    hello();
    foo::hello();
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 2);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::Expression);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);

  const auto& call = dynamic_cast<FunctionCall&>(*script.ast->nodes[0]);
  const auto& call_module = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);

  ASSERT_EQ(call.module, "");
  ASSERT_EQ(call_module.module, "foo");
}

TEST(Ast, Expr_BinaryExprLiterals)
{
  // expressions can only appear in function call args for now.
  // expand when var declaration or assignment is implemented
  const std::string_view src = R"(
    fn foo(a: int) {}
    foo(1+2);
    foo(1+"2");
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 3);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::Expression);
  ASSERT_EQ(script.ast->nodes[2]->node_type(), NodeType::Expression);

  const auto& call_ok = dynamic_cast<FunctionCall&>(*script.ast->nodes[1]);
  ASSERT_EQ(call_ok.args.size(), 1);
  ASSERT_EQ(call_ok.args[0]->node_type(), NodeType::Expression);
  ASSERT_TRUE(call_ok.args[0]->is_expr_type(ExpressionType::Binary));

  const auto& bin_expr_ok = dynamic_cast<BinaryExpression&>(*call_ok.args[0]);
  ASSERT_NE(bin_expr_ok.lhs, nullptr);
  ASSERT_NE(bin_expr_ok.rhs, nullptr);
  ASSERT_EQ(bin_expr_ok.op, BinaryOperator::Add);

  ASSERT_TRUE(bin_expr_ok.lhs->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(bin_expr_ok.rhs->is_expr_type(ExpressionType::Int));

  const auto& lhs_expr = get_expression<IntegerLiteral>(bin_expr_ok.lhs);
  const auto& rhs_expr = get_expression<IntegerLiteral>(bin_expr_ok.rhs);
  ASSERT_EQ(lhs_expr.v, 1);
  ASSERT_EQ(rhs_expr.v, 2);

  // foo(1+"2") ; binary expression with Int + String
  const auto& call_fail = dynamic_cast<FunctionCall&>(*script.ast->nodes[2]);
  const auto& bin_expr_fail = dynamic_cast<BinaryExpression&>(*call_fail.args[0]);

  ASSERT_NE(bin_expr_fail.lhs, nullptr);
  ASSERT_NE(bin_expr_fail.rhs, nullptr);
  ASSERT_EQ(bin_expr_fail.op, BinaryOperator::Add);

  ASSERT_TRUE(bin_expr_fail.lhs->is_expr_type(ExpressionType::Int));
  ASSERT_TRUE(bin_expr_fail.rhs->is_expr_type(ExpressionType::String));

  // const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  // ASSERT_TRUE(func_call_valid(def, call_ok));
  // ASSERT_FALSE(func_call_valid(def, call_fail));
}


TEST(Ast, Expr_BinaryExprFuncs)
{
  const std::string_view src = R"(
    fn foo(a: int) {}
    fn get() -> int {}

    foo(get() + get());
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 3);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::FunctionDef);
  ASSERT_EQ(script.ast->nodes[2]->node_type(), NodeType::Expression);

  const auto& def_foo = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
  ASSERT_EQ(def_foo.params.size(), 1);

  const auto& def_get = dynamic_cast<FunctionDef&>(*script.ast->nodes[1]);
  ASSERT_TRUE(def_get.params.empty());
  ASSERT_EQ(def_get.return_type, "int");

  const auto& call_foo = dynamic_cast<FunctionCall&>(*script.ast->nodes[2]);
  ASSERT_EQ(call_foo.args.size(), 1);
  ASSERT_TRUE(call_foo.args[0]->is_expr_type(ExpressionType::Binary));

  const auto& expr = dynamic_cast<BinaryExpression&>(*call_foo.args[0]);
  ASSERT_TRUE(expr.lhs->is_expr_type(ExpressionType::FuncCall));
  ASSERT_TRUE(expr.rhs->is_expr_type(ExpressionType::FuncCall));
}


// Variables //
TEST(Ast, VariableDecl)
{
  const std::string_view src = R"(
    a: int;
    b: str;
    c: bacon;
  )";

  Parser parser;
  auto script = parser.parse(src);

  ASSERT_EQ(script.ast->nodes.size(), 3);
  ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::VariableDecl);
  ASSERT_EQ(script.ast->nodes[1]->node_type(), NodeType::VariableDecl);
  ASSERT_EQ(script.ast->nodes[2]->node_type(), NodeType::VariableDecl);

  const auto& var_a = dynamic_cast<VariableDecl&>(*script.ast->nodes[0]);
  ASSERT_EQ(var_a.var_name, "a");
  ASSERT_EQ(var_a.var_type, "int");

  const auto& var_b = dynamic_cast<VariableDecl&>(*script.ast->nodes[1]);
  ASSERT_EQ(var_b.var_name, "b");
  ASSERT_EQ(var_b.var_type, "str");

  const auto& var_c = dynamic_cast<VariableDecl&>(*script.ast->nodes[2]);
  ASSERT_EQ(var_c.var_name, "c");
  ASSERT_EQ(var_c.var_type, "bacon");
}


// Syntax Errors //
TEST(Ast, SyntaxError)
{
  {
    Parser parser;
    const std::string_view src = R"(
      fn hello(a: int, b: str) {
    )";

    auto script = parser.parse(src);

    ASSERT_EQ(script.ast->nodes.size(), 1);
    ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);
  }

  {
    Parser parser;
    const std::string_view src = R"(
      fn hello(a: int, b: ) {}
    )";

    auto script = parser.parse(src);

    ASSERT_EQ(script.ast->nodes.size(), 1);
    ASSERT_EQ(script.ast->nodes[0]->node_type(), NodeType::FunctionDef);

    const auto& def = dynamic_cast<FunctionDef&>(*script.ast->nodes[0]);
    ASSERT_EQ(def.name, "hello");

    ASSERT_EQ(def.params.size(), 2);
    ASSERT_EQ(def.params[0].param_name, "a");
    ASSERT_EQ(def.params[0].type_name, "int");

    ASSERT_EQ(def.params[1].param_name, "b");
    ASSERT_EQ(def.params[1].type_name, "");
  }
}
