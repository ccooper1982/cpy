#include <gtest/gtest.h>

#include <cpy/parser.hpp>
#include <cpy/semantics.hpp>

#include "test_common.hpp"


int run_script (const fs::path script)
{
  const auto cmd = std::format ("{} {}", CpyBinary.string(), script.string());

  const int status = std::system(cmd.c_str());

  if (status == -1 || !WIFEXITED(status)) {
    return -1;
  }

  return WEXITSTATUS(status);
}


TEST(ExecExitStatus, Add)
{
  EXPECT_EQ(run_script(TestScriptsDir / "add.cpy"), 3);
}

TEST(ExecExitStatus, Count)
{
  EXPECT_EQ(run_script(TestScriptsDir / "count.cpy"), 42);
}

TEST(ExecExitStatus, Fib)
{
  EXPECT_EQ(run_script(TestScriptsDir / "fib.cpy"), 55);
}
