#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>


int main ([[maybe_unused]] int argc, [[maybe_unused]] char ** argv)
{
  const std::string_view src = R"(
    fn hello(a: int, b: ) {}
  )";

  Parser parser;
  const auto script = parser.parse(src);

  script.issues.dump(std::cout, script.src);

  return script.issues.have_errors() ? 1 : 0;
}
