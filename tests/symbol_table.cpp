#include <cpy/ast/ast_node.hpp>
#include <cpy/ast/expressions.hpp>
#include <gtest/gtest.h>

#include <cpy/ast/ast.hpp>
#include <cpy/issues.hpp>
#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>


TEST(SymbolTable, FunctionDef)
{
  const std::string_view src = R"(
    fn foo(a: int) {}
    fn bar(a: str, b: dec) -> bool {}
    fn baz(a: bacon) -> cheese {}
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  const auto& sym_table = sem.symbol_table();

  // baz not stored because 'bacon' and 'cheese' are unknown types
  ASSERT_TRUE(sym_table.has_function("foo"));
  ASSERT_TRUE(sym_table.has_function("bar"));
  ASSERT_FALSE(sym_table.has_function("baz"));

  const auto& resolved_foo = sym_table.get_function("foo");
  ASSERT_EQ(resolved_foo.params.size(), 1);
  ASSERT_EQ(resolved_foo.params[0].name, "a");
  ASSERT_EQ(resolved_foo.params[0].type, VarType{BuiltInType::Int});
  ASSERT_EQ(resolved_foo.return_type, VarType{BuiltInType::Void});

  const auto& resolved_bar = sym_table.get_function("bar");
  ASSERT_EQ(resolved_bar.params.size(), 2);
  ASSERT_EQ(resolved_bar.params[0].name, "a");
  ASSERT_EQ(resolved_bar.params[1].name, "b");
  ASSERT_EQ(resolved_bar.params[0].type, VarType{BuiltInType::String});
  ASSERT_EQ(resolved_bar.params[1].type, VarType{BuiltInType::Decimal});
  ASSERT_EQ(resolved_bar.return_type, VarType{BuiltInType::Bool});
}


TEST(SymbolTable, VarDecl)
{
  {
    const std::string_view src = R"(
      fn foo() -> int { return 1; }

      a := 5;
      b := foo();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_FALSE(script.issues->have_errors());
    ASSERT_TRUE(sym_table.has_function("foo"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_TRUE(sym_table.scope_has_variable("b", script.ast->block));

    ASSERT_EQ(sym_table.get_function("foo").return_type, VarType{BuiltInType::Int});

    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
    ASSERT_EQ(sym_table.get_variable("b", script.ast->block).type, VarType{BuiltInType::Int});
  }

  {
    // can't initialise variable from function returning void
    const std::string_view src = R"(
      fn foo1() {}
      fn foo2() -> void {}

      a := foo1();
      b := foo2();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_FALSE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_FALSE(sym_table.scope_has_variable("b", script.ast->block));

    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableInitVoid), 2);
  }

  {
    const std::string_view src = R"(
      fn foo1() -> int {}
      fn foo2() {}
      fn foo3() -> str {}

      a := foo1() + foo1();
      b := foo1() + foo2();
      c := foo1() + foo3();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.has_function("foo3"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_FALSE(sym_table.scope_has_variable("b", script.ast->block));
    ASSERT_FALSE(sym_table.scope_has_variable("c", script.ast->block));

    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
    ASSERT_EQ(script.issues->get_count(ErrorCode::BinaryOperandsInvalid), 1);
    ASSERT_EQ(script.issues->get_count(ErrorCode::BinaryOperandsVoid), 1);
  }
}


TEST(SymbolTable, VarDecl_Operation)
{
  const std::string_view src = R"(
    a := 4 + 1;
    b := 1 < 5;
    c: int = "a";
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  const auto& sym_table = sem.symbol_table();

  ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
  ASSERT_TRUE(sym_table.scope_has_variable("b", script.ast->block));
  ASSERT_FALSE(sym_table.scope_has_variable("c", script.ast->block));

  ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
  ASSERT_EQ(sym_table.get_variable("b", script.ast->block).type, VarType{BuiltInType::Bool});
}


TEST(SymbolTable, VarDecl_NestedFuncCall)
{
  {
    const std::string_view src = R"(
      fn foo1() -> int {}
      fn foo2(a: int) {}

      a := foo2(foo1());
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_FALSE(sym_table.scope_has_variable("a", script.ast->block));

    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableInitVoid), 1);
  }

  {
    const std::string_view src = R"(
      fn foo1() -> str {}
      fn foo2(a: str) -> str {}

      a := foo2(foo1());
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::String});
  }

  {
    const std::string_view src = R"(
      fn foo1(a: str) -> int {}
      fn foo2(a: int) -> str {}
      fn foo3(a: str) -> int {}

      a := foo1(foo2(foo3("recursing")));
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.has_function("foo3"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int, b: str) -> int {}
      fn foo2() -> int {}
      fn foo3() -> str {}

      a := foo1(foo2(), foo3());
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.has_function("foo3"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int, b: str) -> int {}
      fn foo2() -> int {}
      fn foo3() -> str {}
      fn foo4(a: int, b: str, c: int) -> int {}

      a := foo4(foo2(), foo3(), foo1(foo2(), foo3()));
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.has_function("foo3"));
    ASSERT_TRUE(sym_table.has_function("foo4"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
  }
}


TEST(SymbolTable, VarDecl_BinExpr)
{
  const std::string_view src = R"(
    fn foo1(a: int) -> int {}
    fn foo2() -> int {}
    fn foo3() -> int {}
    fn foo4() -> str {}

    a := foo1(foo2() + foo3());
    b := foo1(foo2() + foo4());
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  const auto& sym_table = sem.symbol_table();

  ASSERT_TRUE(sym_table.has_function("foo1"));
  ASSERT_TRUE(sym_table.has_function("foo2"));
  ASSERT_TRUE(sym_table.has_function("foo3"));
  ASSERT_TRUE(sym_table.has_function("foo4"));
  ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
  ASSERT_FALSE(sym_table.scope_has_variable("b", script.ast->block));

  ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
  ASSERT_EQ(script.issues->get_count(ErrorCode::BinaryOperandsInvalid), 1);
}


TEST(SymbolTable, VariableDecl_Scope)
{
  {
    const std::string_view src = R"(
      fn foo()
      {
        a: int;
        b: int;
      }

      a: int;
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }
}

TEST(SymbolTable, VariableDecl_ScopeIf)
{
  {
    const std::string_view src = R"(
      a: int;

      fn foo()
      {
        b: int;

        if (a > b) {
        }
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }

  {
    const std::string_view src = R"(
      fn foo()
      {
        b: int;
      }

      if (b > 5) {

      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableUnknown), 1);
  }

  {
    const std::string_view src = R"(
      fn foo()
      {
        a: int;
      }

      b: int;

      if (a > b){
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableUnknown), 1);
    ASSERT_EQ(script.issues->get_count(ErrorCode::ConditionNotBool), 1);
  }

  {
    const std::string_view src = R"(
      a: bool;
      b: int;

      if (a)
      {
        c: int;
        if (b > c) {
          a: str;
        }
      }

      c: str;
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());

    const auto& root_block = script.ast->block;
    const auto& first_if = dynamic_cast<ConditionIf&>(*root_block.nodes[2]);
    const auto& second_if = dynamic_cast<ConditionIf&>(*first_if.if_.body.nodes[1]);

    ASSERT_EQ(sem.symbol_table().get_variable("a", root_block).type, BuiltInType::Bool);
    ASSERT_EQ(sem.symbol_table().get_variable("b", root_block).type, BuiltInType::Int);
    ASSERT_EQ(sem.symbol_table().get_variable("c", root_block).type, BuiltInType::String);
    ASSERT_EQ(sem.symbol_table().get_variable("c", first_if.if_.body).type, BuiltInType::Int);
    ASSERT_EQ(sem.symbol_table().get_variable("a", second_if.if_.body).type, BuiltInType::String);
  }
}

TEST(SymbolTable, VariableDecl_IfElseIf)
{
  {
    const std::string_view src = R"(
      a: bool;
      b: int;

      if (a) {
      }
      else if (b == 0) {
      }
      else if (b == 1) {
      }
      else {
      }
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());

    const auto& root = script.ast->block;

    const auto& if_ = dynamic_cast<ConditionIf&>(*root.nodes[2]);

    ASSERT_EQ(if_.else_if_count(), 2);
    ASSERT_TRUE(if_.have_else());
  }
}

TEST(SymbolTable, VariableDecl_ScopeIfElseIf)
{
  {
    const std::string_view src = R"(
      a: bool;

      if (a)
      {
        b: int;
        c: bool;

        if (a) {
          c: int;
        }
        else if (c) {
          c: str;
        }
      }
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());

    const auto& root = script.ast->block;

    const auto& first_if = dynamic_cast<ConditionIf&>(*root.nodes[1]);
    ASSERT_EQ(first_if.if_body_node_count(), 3); // b, c and if

    const auto& second_if = dynamic_cast<ConditionIf&>(*first_if.if_body().nodes[2]);
    ASSERT_EQ(second_if.else_if_count(), 1);
    ASSERT_EQ(second_if.else_if[0].body.nodes.size(), 1); // c: str

    ASSERT_EQ(sem.symbol_table().get_variable("a", root).type, BuiltInType::Bool);
    ASSERT_EQ(sem.symbol_table().get_variable("b", first_if.if_.body).type, BuiltInType::Int);
    ASSERT_EQ(sem.symbol_table().get_variable("c", first_if.if_.body).type, BuiltInType::Bool);
    ASSERT_EQ(sem.symbol_table().get_variable("c", second_if.if_.body).type, BuiltInType::Int);
    ASSERT_EQ(sem.symbol_table().get_variable("c", second_if.else_if[0].body).type, BuiltInType::String);
  }
}


TEST(SymbolTable, FuncCall_VarRef)
{
  {
    const std::string_view src = R"(
      fn foo1(a: int) -> int {}

      a: int;
      foo1(a);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int){}

      a: str;
      foo1(a);
      foo1(b);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));

    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::String});
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionCallArgType), 1);
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableUnknown), 1);
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int, b: str) -> int {}

      a: int;
      b: str;
      foo1(a, b);
      foo1(b, a);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_TRUE(sym_table.scope_has_variable("b", script.ast->block));

    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
    ASSERT_EQ(sym_table.get_variable("b", script.ast->block).type, VarType{BuiltInType::String});
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionCallArgType), 1);
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int) -> int {}
      fn foo2(a: int, b: str) -> int {}

      a: int;
      b: str;

      foo1(a+a);
      foo2(foo1(a), "abc");
      foo2(foo1(a), b);
      foo1(a+b);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.has_function("foo2"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
    ASSERT_TRUE(sym_table.scope_has_variable("b", script.ast->block));
    ASSERT_TRUE(script.issues->have_errors());

    ASSERT_EQ(sym_table.get_variable("a", script.ast->block).type, VarType{BuiltInType::Int});
    ASSERT_EQ(sym_table.get_variable("b", script.ast->block).type, VarType{BuiltInType::String});
    ASSERT_EQ(script.issues->get_count(ErrorCode::BinaryOperandsInvalid), 1);
  }
}


TEST(SymbolTable, FuncCall_VarAssign)
{
  // not much to do here: variable assignment is mostly dealt with by
  // the AST and Semantics tests.
  {
    const std::string_view src = R"(
      fn foo1(a: int) -> int {}

      a: int;
      a = 5;
      a = foo1(1);
      a = foo1(1) + foo1(2);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.has_function("foo1"));
    ASSERT_TRUE(sym_table.scope_has_variable("a", script.ast->block));
  }

  {
    const std::string_view src = R"(
      a: int;
      b: int;

      a = 1;

      if (a > b)
      {
        a: bool;
        a = true;
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();
    const auto& root = script.ast->block;
    const auto& if_ = dynamic_cast<const ConditionIf&>(*root.nodes[3]).if_body();

    ASSERT_TRUE(sym_table.scope_has_variable("a", root));
    ASSERT_TRUE(sym_table.scope_has_variable("a", if_));

    ASSERT_EQ(sym_table.get_variable("a", root).type, BuiltInType::Int);
    ASSERT_EQ(sym_table.get_variable("a", if_).type, BuiltInType::Bool);

    const auto& assign_root = dynamic_cast<const VariableAssign&>(*root.nodes[2]);
    ASSERT_TRUE(assign_root.expr->is_expr_type(ExpressionType::Int));

    const auto& assign_if = dynamic_cast<const VariableAssign&>(*if_.nodes[1]);
    ASSERT_TRUE(assign_if.expr->is_expr_type(ExpressionType::Bool));
  }
}
