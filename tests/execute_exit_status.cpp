#include <gtest/gtest.h>

#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>

#include "test_common.hpp"


template<typename ...Args>
int run_script (const fs::path script, Args... args)
{
  auto cmd = std::format ("{} {}", CpyBinary.string(), script.string());
  ((cmd += std::format(" {}", std::forward<Args>(args))), ...);

  const int status = std::system(cmd.c_str());

  if (status == -1 || !WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}

// NOTE: exit status value range is [0,255]. But these test are sanity checks without
//       requiring pipes to read stdout/err

TEST(ExecExitStatus, Add)
{
  EXPECT_EQ(run_script(TestScriptsDir / "add.cpy 1 2"), 3);
  EXPECT_EQ(run_script(TestScriptsDir / "add.cpy", 10, -2), 8);
}

TEST(ExecExitStatus, Count)
{
  // this test isn't about counting, it was to test the stack is correct with
  // recursive function calls , a loop and if
  EXPECT_EQ(run_script(TestScriptsDir / "count.cpy"), 42);
}

TEST(ExecExitStatus, Fib_Recursive)
{
  EXPECT_EQ(run_script(TestScriptsDir / "fib_recursive.cpy", 13), 233);
}

TEST(ExecExitStatus, Fib_Iterative)
{
  EXPECT_EQ(run_script(TestScriptsDir / "fib_iterative.cpy", 13), 233);
}
