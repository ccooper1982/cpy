#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>

int main ([[maybe_unused]] int argc, [[maybe_unused]] char ** argv)
{
  const std::string_view src = R"(
    fn hello(a: int, b: str, c: dec, d: bool) {}

    hello(1, "one", 1.1, true);
  )";

  fs::path file;
  if (argc == 2) {
    file = argv[1];
  }

  Parser parser;
  const auto script = parser.parse(file.empty() ? src : file);

  if (script)
  {
    script->issues.dump(std::cout, script->src);
    return script->issues.have_errors() ? 1 : 0;
  }
  else
  {
    std::cout << script.error().msg() << "\n";
  }

  return 1;
}
