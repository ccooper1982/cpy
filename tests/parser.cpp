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

  ASSERT_NO_THROW(parser.parse(std::string_view{"hello();"}));
  ASSERT_FALSE(parser.ast()->nodes.empty());
}

TEST(Parser, File)
{
  {
    Parser parser;

    ASSERT_TRUE(parser.parse(TestDataDir / "simple.cpy"));
    ASSERT_FALSE(parser.ast()->nodes.empty());
  }

  {
    Parser parser;
    ASSERT_FALSE(parser.parse(TestDataDir / "dont_exist.cpy"));
  }
}
