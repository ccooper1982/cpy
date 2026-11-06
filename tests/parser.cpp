#include <filesystem>
#include <gtest/gtest.h>

#include <cpy/parser.hpp>
#include <stdexcept>

/*
 * This just tests the parser can read from string_view or from filesystem::path.
 * It doesn't test the AST in depth, only that nothing breaks.
 */

static const auto TestScriptsDir = fs::path{"tests/scripts"};

TEST(Parser, String)
{
  Parser parser;
  Script script;

  ASSERT_NO_THROW(script = parser.parse(std::string_view{"hello();"}));
  ASSERT_EQ(script.ast->block.node_count(), 1);
}

TEST(Parser, File_Exist)
{
  Parser parser;
  Script script;

  ASSERT_NO_THROW(script = parser.parse(TestScriptsDir / "simple.cpy"));
  ASSERT_TRUE(script.ast->block.node_count() > 0);
}

TEST(Parser, File_NotExist)
{
  Parser parser;
  ASSERT_THROW(parser.parse(TestScriptsDir / "dont_exist.cpy"), std::runtime_error);
}
