#include <filesystem>
#include <gtest/gtest.h>

#include <cpy/parser.hpp>
#include <stdexcept>

/*
 * This just tests the parser can read from string_view or from filesystem::path.
 * It doesn't test the AST in depth, only that nothing breaks.
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
    Script script;

    ASSERT_NO_THROW(script = parser.parse(TestDataDir / "simple.cpy"));
    ASSERT_FALSE(script.ast->nodes.empty());
  }

  {
    Parser parser;
    ASSERT_THROW(parser.parse(TestDataDir / "dont_exist.cpy"), std::runtime_error);
  }
}
