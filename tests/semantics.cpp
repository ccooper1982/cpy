
#include <gtest/gtest.h>

#include <cpy/issues.hpp>
#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>


TEST(Semantics, Ok)
{
  {
    Parser parser;
    auto script = parser.parse(std::string_view{});

    Semantics sem;
    sem.process(script);
    ASSERT_FALSE(script.issues->have_errors());
  }

  {
    const std::string_view src = R"(
      fn hello() {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }

  {
    const std::string_view src = R"(
      fn hello() {}

      hello();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }
}

TEST(Semantics, VarUndeclared)
{
  {
    const std::string_view src = R"(
      a = 5;
      a: int;
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableUnknown), 1);
  }

  {
    const std::string_view src = R"(
      fn hello()
      {
        a = 5;
      }

      a: int;
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableUnknown), 1);
  }
}


TEST(Semantics, FuncDef)
{
  {
    const std::string_view src = R"(
      fn hello(a: bacon) {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::UnknownParamType), 1);
  }

  {
    // ErrorCode::UnknownParamType count is 1 because semantics skip when
    // when the first error is found on the function def
    const std::string_view src = R"(
      fn hello(a: bacon, b: cheese) {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::UnknownParamType), 1);
  }

  // return type
  {
    const std::string_view src = R"(
      fn hello(a: int) -> bacon {}
      fn hello2(a: int) -> cheese {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::UnknownReturnType), 2);
  }

  {
    const std::string_view src = R"(
      fn hello(a: int) {}
      fn hello(a: int) {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionDuplicate), 1);
  }

  // overloading not permitted yet
  {
    const std::string_view src = R"(
      fn hello(a: int) {}
      fn hello(b: str) {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionDuplicate), 1);
  }
}


TEST(Semantics, FuncCall)
{
  {
    const std::string_view src = R"(
      fn hello(a: int) {}

      hello();
      hello(1,2,3);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionCallArgsCount), 2);
  }

  {
    const std::string_view src = R"(
      fn hello(a: int) {}

      hello("str");
      hello(true);
      hello(1.5);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionCallArgType), 3);
  }

  {
    const std::string_view src = R"(
      module_name_not_exist::hello();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::ModuleNotExist), 1);
  }
}


TEST(Semantics, Func_Return)
{
  {
    const std::string_view src = R"(
      fn foo1() -> int {}
      fn foo2() {}
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionReturnExprExpected), 1);
  }

  {
    const std::string_view src = R"(
      fn foo1() -> int { return "one"; }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionReturnExpression), 1);
  }
}

TEST(Semantics, VariableDecl)
{
  {
    const std::string_view src = R"(
      a: int;
      b: bacon;
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::UnknownVarType), 1);
  }

  {
    const std::string_view src = R"(
      a: int;
      b: bacon;

      fn foo()
      {
        c: cheese;
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::UnknownVarType), 2);
  }

  {
    const std::string_view src = R"(
      fn foo() {}
      a := foo();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableInitVoid), 1);
  }

  {
    const std::string_view src = R"(
      fn foo() -> int { return 1; }
      a := foo();
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }

  {
    const std::string_view src = R"(
      fn foo1(a: int) -> int { return 1; }

      a: int;
      foo1(a);
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_FALSE(script.issues->have_errors());
  }
}


TEST(Semantics, Condition)
{
  {
    const std::string_view src = R"(
      a: int;
      b: int;

      if (a > b)
      {
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
      a: int;
      b: int;

      if (a)
      {
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::ConditionNotBool), 1);
  }

  {
    const std::string_view src = R"(
      a: int;
      b: int;

      if (a > b)
      {
        foo();
      }
    )";

    Parser parser;
    auto script = parser.parse(src);

    Semantics sem;
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::FunctionNotExist), 1);
  }
}


TEST(Semantics, VarAssign)
{
  {
    const std::string_view src = R"(
      a: int;
      a = 5;
      a = "asda";
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableAssignType), 1);
  }

  {
    const std::string_view src = R"(
      fn foo() {}

      a: int;
      a = foo();
    )";

    Parser parser;
    Semantics sem;

    auto script = parser.parse(src);
    sem.process(script);

    ASSERT_TRUE(script.issues->have_errors());
    ASSERT_EQ(script.issues->get_count(ErrorCode::VariableAssignType), 1);
  }
}
