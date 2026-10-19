#include <gtest/gtest.h>
#include <cpy/common.hpp>
#include <cpy/exec/executor.hpp>
#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>
#include <cpy/symbol_table.hpp>
#include <string_view>


TEST(Exec, Basic)
{
  {
    const std::string_view src = R"(
      a :int;
      b :int = 1;
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
    {
      ASSERT_EQ(stack_size, 1);
      ASSERT_EQ(vars.size(), 2);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{0}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyInt{1}});
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }
}

TEST(Exec, FuncCall_NoParams_Empty)
{
  const std::string_view src = R"(
    a :int = 1;

    fn foo() {}
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_push = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    ASSERT_EQ(stack_size, 1);
    ASSERT_EQ(vars.size(), 1);
  };

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    ASSERT_EQ(stack_size, 1);
    ASSERT_EQ(vars.size(), 1);
    ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{1}});
  };

  TestExecutor exec(sem.symbol_table(), on_push, on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_NoParams_Local)
{
  const std::string_view src = R"(
    a :int = 1;

    fn foo()
    {
      b: int = a;
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_push = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 1)
    {
      ASSERT_EQ(stack_size, 1);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{0}});
    }
  };

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 2);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{1}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyInt{1}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_push, on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ParamsAssign)
{
  const std::string_view src = R"(
    foo(10, true);

    fn foo(a: int, b: bool)
    {
      a = a * 2;
      b = false;
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_push = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 2);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyBool{true}});
    }
  };

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 2);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{20}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyBool{false}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_push, on_pop);
  exec.execute(script.ast->block);
}

TEST(Exec, FuncCall_Params)
{
  const std::string_view src = R"(
    foo(10, true);

    fn foo(a: int, b: bool)
    {
      x := a;
      y := b;
      z := x * 2;
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 5);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyBool{true}});
      ASSERT_EQ(vars.at("x").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("y").value, VarValue{CpyBool{true}});
      ASSERT_EQ(vars.at("z").value, VarValue{CpyInt{20}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ParamsWithParent)
{
  const std::string_view src = R"(
    z := 20;

    foo(10, true);

    fn foo(a: int, b: bool)
    {
      x := a;
      y := b;
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 4);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyBool{true}});
      ASSERT_EQ(vars.at("x").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("y").value, VarValue{CpyBool{true}});
    }
    else if (stack_size == 1)
    {
      ASSERT_EQ(vars.size(), 1);
      ASSERT_EQ(vars.at("z").value, VarValue{CpyInt{20}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ParamsWithReturn)
{
  const std::string_view src = R"(
    z := foo(10, 20);

    fn foo(a: int, b: int) -> int
    {
      return a+b;
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 2)
    {
      ASSERT_EQ(vars.size(), 2);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyInt{10}});
      ASSERT_EQ(vars.at("b").value, VarValue{CpyInt{20}});
    }
    else if (stack_size == 1)
    {
      ASSERT_EQ(vars.size(), 1);
      ASSERT_EQ(vars.at("z").value, VarValue{CpyInt{30}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_AssignParent)
{
  const std::string_view src = R"(
    a := "hello";
    foo();
    a = "cpy";

    fn foo()
    {
      a = "from"
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::map<VarName, Variable> vars, const size_t stack_size)
  {
    if (stack_size == 1)
    {
      ASSERT_EQ(vars.size(), 1);
      ASSERT_EQ(vars.at("a").value, VarValue{CpyString{"cpy"}});
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}
