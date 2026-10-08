#include "cpy/ast/ast_node.hpp"
#include "cpy/issues.hpp"
#include <gtest/gtest.h>

#include <cpy/semantics.hpp>
#include <cpy/parser.hpp>

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
  ASSERT_TRUE(sym_table.have_function("foo"));
  ASSERT_TRUE(sym_table.have_function("bar"));
  ASSERT_FALSE(sym_table.have_function("baz"));

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
      fn foo() -> int {}

      a := 5;
      b := foo();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    const auto& sym_table = sem.symbol_table();

    ASSERT_TRUE(sym_table.have_function("foo"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_TRUE(sym_table.have_variable("b"));

    ASSERT_EQ(sym_table.get_function("foo").return_type, VarType{BuiltInType::Int});

    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
    ASSERT_EQ(sym_table.get_variable("b").type, VarType{BuiltInType::Int});
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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_FALSE(sym_table.have_variable("a"));
    ASSERT_FALSE(sym_table.have_variable("b"));

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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_TRUE(sym_table.have_function("foo3"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_FALSE(sym_table.have_variable("b"));
    ASSERT_FALSE(sym_table.have_variable("c"));

    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableInitBinaryInvalid), 2);
  }
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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_FALSE(sym_table.have_variable("a"));

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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::String});
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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_TRUE(sym_table.have_function("foo3"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_TRUE(sym_table.have_function("foo3"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
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

    ASSERT_TRUE(sym_table.have_function("foo1"));
    ASSERT_TRUE(sym_table.have_function("foo2"));
    ASSERT_TRUE(sym_table.have_function("foo3"));
    ASSERT_TRUE(sym_table.have_function("foo4"));
    ASSERT_TRUE(sym_table.have_variable("a"));
    ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
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

  ASSERT_TRUE(sym_table.have_function("foo1"));
  ASSERT_TRUE(sym_table.have_function("foo2"));
  ASSERT_TRUE(sym_table.have_function("foo3"));
  ASSERT_TRUE(sym_table.have_function("foo4"));
  ASSERT_TRUE(sym_table.have_variable("a"));
  ASSERT_FALSE(sym_table.have_variable("b"));

  ASSERT_EQ(sym_table.get_variable("a").type, VarType{BuiltInType::Int});
  ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionCallArgBinaryInvalid), 1);
}
