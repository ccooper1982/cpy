#include <filesystem>
#include <gtest/gtest.h>

#include <cpy/parser.hpp>

/*
 * This just tests the parser can read from string_view or from filesystem::path.
 * It doesn't test the AST in depth, only that something happened.
 */

static const auto TestDataDir = fs::path{"tests/src"};

TEST(Parser, String)
{
  Parser parser;
  Script script;

  ASSERT_NO_THROW(script = parser.parse(std::string_view{"hello();"}));
  ASSERT_FALSE(script.ast->nodes.empty());
}

TEST(Parser, File)
{
  {
    Parser parser;
    const auto parsed = parser.parse(TestDataDir / "simple.cpy");

    ASSERT_TRUE(parsed.has_value());
    ASSERT_FALSE(parsed->ast->nodes.empty());
  }

  {
    Parser parser;
    const auto parsed = parser.parse(TestDataDir / "dont_exist.cpy");

    ASSERT_FALSE(parsed.has_value());
  }
}
