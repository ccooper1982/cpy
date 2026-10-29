#include "cpy/exec/operators.hpp"
#include <gtest/gtest.h>
#include <cpy/common.hpp>
#include <cpy/exec/executor.hpp>
#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>
#include <cpy/symbol_table.hpp>
#include <string_view>
#include <type_traits>


template<typename T>
VarValue make_value (T v)
{
  if constexpr (std::is_same_v<T, CpyBool>) {
    return CpyBool(v);
  }
  else if constexpr (std::is_integral_v<T>) {
    return CpyInt(v);
  }
  else if constexpr (std::floating_point<T>) {
    return CpyDec(v);
  }
  else if constexpr (std::is_convertible_v<T, CpyString>) {
    return CpyString(v);
  }
  else {
    static_assert(false, "Unsupported VarValue type");
  }
}

TEST(Exec, Basic)
{
  const std::string_view src = R"(
    a :int;
    b :int = 1;
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    ASSERT_EQ(stack.size(), 1);
    ASSERT_EQ(stack[0].size(), 2);
    ASSERT_EQ(stack[0].at("a").value(), make_value(0));
    ASSERT_EQ(stack[0].at("b").value(), make_value(1));
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}

TEST(Exec, FuncCall_NoParams_Empty)
{
  const std::string_view src = R"(
    a :int = 1;
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

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    ASSERT_EQ(stack.size(), 1);
    ASSERT_EQ(stack[0].size(), 1);
    ASSERT_EQ(stack[0].at("a").value(), make_value(1));
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
      ASSERT_EQ(vars.at("a").value(), make_value(0));
    }
  };

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 1) {
      ASSERT_EQ(stack.size(), 1);
    }
    else if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      ASSERT_EQ(stack[0].at("a").value(), make_value(1));
      ASSERT_EQ(stack[1].at("b").value(), make_value(1));
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
      ASSERT_EQ(vars.at("a").value(), make_value(10));
      ASSERT_EQ(vars.at("b").value(), make_value(true));
    }
  };

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      ASSERT_EQ(stack[1].at("a").value(), make_value(20));
      ASSERT_EQ(stack[1].at("b").value(), make_value(false));
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

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      ASSERT_EQ(stack[1].at("a").value(), make_value(10));
      ASSERT_EQ(stack[1].at("b").value(), make_value(true));
      ASSERT_EQ(stack[1].at("x").value(), make_value(10));
      ASSERT_EQ(stack[1].at("y").value(), make_value(true));
      ASSERT_EQ(stack[1].at("z").value(), make_value(20));
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

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[1].size(), 4);
      ASSERT_EQ(stack[1].at("a").value(), make_value(10));
      ASSERT_EQ(stack[1].at("b").value(), make_value(true));
      ASSERT_EQ(stack[1].at("x").value(), make_value(10));
      ASSERT_EQ(stack[1].at("y").value(), make_value(true));
    }
    else if (stack.size() == 1)
    {
      ASSERT_EQ(stack.size(), 1);
      ASSERT_EQ(stack[0].at("z").value(), make_value(20));
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

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      ASSERT_EQ(stack[1].size(), 2);
      ASSERT_EQ(stack[1].at("a").value(), make_value(10));
      ASSERT_EQ(stack[1].at("b").value(), make_value(20));
    }
    else if (stack.size() == 1)
    {
      ASSERT_EQ(stack.size(), 1);
      ASSERT_EQ(stack[0].at("z").value(), make_value(30));
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
      a = "from";
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack.size(), 2);
      // `a` is in parent
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[0].at("a").value(), make_value("from"));
    }
    else if (stack.size() == 1)
    {
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[0].at("a").value(), make_value("cpy"));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_If)
{
  const std::string_view src = R"(
    fn foo(a: int)
    {
      return a*2;
    }

    a := true;
    z: int;

    if (a) {
      z = foo(1);
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    // if (a)
    if (stack.size() == 2) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
    // end of script/return from if(a)
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_Else)
{
  const std::string_view src = R"(
    fn foo(a: int)
    {
      return a*2;
    }

    a := false;
    z: int;

    if (a) {

    }
    else {
     z = foo(1);
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    // if (a)
    if (stack.size() == 2) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
    // end of script/return from if(a)
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ElseIf)
{
  const std::string_view src = R"(
    fn foo(a: int)
    {
      return a*2;
    }

    a := false;
    z: int;

    if (a) {
    }
    else if (z == 0) {
      z = foo(1);
    }
    else {
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    // if (a)
    if (stack.size() == 2) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
    // end of script/return from if(a)
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_IfLocal)
{
  const std::string_view src = R"(
    fn foo(a: int) -> int
    {
      return a*2;
    }

    a := true;
    z: int;

    if (a)
    {
      z = foo(1);
      zz := foo(2);
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    // if (a)
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack[0].size(), 2);
      ASSERT_EQ(stack[1].size(), 1);
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
      ASSERT_EQ(stack[1].at("zz").value(), make_value(4));
    }
    // end of script/return from if(a)
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("z").value(), make_value(2));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ConditionVarious)
{
  // not pretty because we must escape formatting '{' and '}' placeholders
  static const std::string src_template = R"(
    fn foo(a: int) -> int
    {{
      return a*2;
    }}

    a :bool = {};
    b :int = {};
    c :int = {};
    z :int = {};

    if (a) {{
      z = foo(1);
    }}
    else if (b == c) {{
      z = foo(2);
    }}
    else if (b > c) {{
      z = foo(3);
    }}
    else {{
      z = foo(4);
    }}
  )";

  auto make_script = [](const std::string_view a, const std::string_view b, const std::string_view c, const std::string_view z) -> std::string
  {
    return std::vformat(src_template, std::make_format_args(a, b, c, z));
  };

  {
    Parser parser;
    auto script = parser.parse(std::string_view{make_script("true", "0", "0", "0")});

    Semantics sem;
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      ASSERT_EQ(stack[0].size(), 4);

      // if (a)
      if (stack.size() == 2) {
        ASSERT_EQ(stack[0].at("z").value(), make_value(2));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    Parser parser;
    auto script = parser.parse(std::string_view{make_script("false", "0", "0", "0")});

    Semantics sem;
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      // else if (a == b)
      if (stack.size() == 2) {
        ASSERT_EQ(stack[0].at("z").value(), make_value(4));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    Parser parser;
    auto script = parser.parse(std::string_view{make_script("false", "2", "1", "0")});

    Semantics sem;
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      // else if (a > b)
      if (stack.size() == 2) {
        ASSERT_EQ(stack[0].at("z").value(), make_value(6));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    Parser parser;
    auto script = parser.parse(std::string_view{make_script("false", "2", "3", "0")});

    Semantics sem;
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      // else
      if (stack.size() == 2) {
        ASSERT_EQ(stack[0].at("z").value(), make_value(8));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }
}


TEST(Exec, FuncCall_ReturnFromIf)
{
  const std::string_view src = R"(
    a := foo();

    fn foo() -> int
    {
      if (true) {
          return 5;
      }
    }
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    // if (a)
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[1].size(), 0);
    }
    // end of script/return from foo() call
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("a").value(), make_value(5));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ConditionDelcVarInIf)
{
  const std::string_view src = R"(
    a := true;

    if (a) {
      b := 1;
    }

    a = false;
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 2)
    {
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[1].size(), 1);
      ASSERT_EQ(stack[0].at("a").value(), make_value(true));
      ASSERT_EQ(stack[1].at("b").value(), make_value(1));
    }
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("a").value(), make_value(false));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ConditionDelcVarInNestedIf)
{
  const std::string_view src = R"(
    a := true;

    if (a)
    {
      if (false) {

      }
      else {
        b := true;
      }
    }

    a = false;
  )";

  Parser parser;
  auto script = parser.parse(src);

  Semantics sem;
  sem.process(script);

  auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
  {
    if (stack.size() == 3) {
      ASSERT_EQ(stack[2].at("b").value(), make_value(true));
    }
    else if (stack.size() == 2)
    {
      ASSERT_EQ(stack[0].size(), 1);
      ASSERT_EQ(stack[1].size(), 0);
      ASSERT_EQ(stack[0].at("a").value(), make_value(true));
    }
    else if (stack.size() == 1) {
      ASSERT_EQ(stack[0].at("a").value(), make_value(false));
    }
  };

  TestExecutor exec(sem.symbol_table(), on_pop);
  exec.execute(script.ast->block);
}


TEST(Exec, FuncCall_ParamRef)
{
  {
    const std::string_view src = R"(
      fn foo(x: ref int)
      {
        x = 5;
      }

      a := 1 ;

      foo(ref a);
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      if (stack.size() == 2)
      {
        ASSERT_EQ(stack[0].size(), 1);
        ASSERT_EQ(stack[1].size(), 1);
      }
      else if (stack.size() == 1) {
        ASSERT_EQ(stack[0].at("a").value(), make_value(5));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    const std::string_view src = R"(
      fn foo(x: ref str, y: int)
      {
        x = "world";
        y = 2;
      }

      a := "hello";
      b := 1;

      foo(ref a, b);
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      if (stack.size() == 2)
      {
        ASSERT_EQ(stack[0].size(), 2);
        ASSERT_EQ(stack[1].size(), 2);
        ASSERT_EQ(stack[1].at("y").value(), make_value(2));
      }
      else if (stack.size() == 1)
      {
        ASSERT_EQ(stack[0].at("a").value(), make_value("world"));
        ASSERT_EQ(stack[0].at("b").value(), make_value(1));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    const std::string_view src = R"(
      fn foo(x: ref str, y: int)
      {
        x = bar();
        y = 2;
      }

      fn bar() -> str
      {
        return "world";
      }

      a := "hello";
      b := 1;

      foo(ref a, b);
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      if (stack.size() == 2)
      {
        ASSERT_EQ(stack[0].size(), 2);
        ASSERT_EQ(stack[1].size(), 2);
        ASSERT_EQ(stack[1].at("y").value(), make_value(2));
      }
      else if (stack.size() == 1)
      {
        ASSERT_EQ(stack[0].at("a").value(), make_value("world"));
        ASSERT_EQ(stack[0].at("b").value(), make_value(1));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }

  {
    const std::string_view src = R"(
      fn foo(x: ref str)
      {
        bar(ref x);
      }

      fn bar(xx: ref str)
      {
        xx = "world";
      }

      a := "hello";

      foo(ref a);
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    auto on_pop = [&](const std::vector<std::map<VarName, Variable>> stack)
    {
      if (stack.size() == 1) {
        ASSERT_EQ(stack[0].at("a").value(), make_value("world"));
      }
    };

    TestExecutor exec(sem.symbol_table(), on_pop);
    exec.execute(script.ast->block);
  }
}
