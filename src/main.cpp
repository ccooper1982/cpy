#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>

int main (int argc, char ** argv)
{
  const std::string_view src = R"(
    fn hello(a: int){}
    hello(1+2);
    hello(1.5+2);
    hello(1.5+true);
  )";

  fs::path file;
  if (argc == 2) {
    file = argv[1];
  }

  Modules::initialise();

  Parser parser;

  if (!file.empty())
  {
    if (!parser.parse(file))
    {
      std::cout << "Failed to open " << file.string() << '\n';
      return 1;
    }
  }
  else {
    parser.parse(src);
  }

  parser.ast()->dump(std::cout);
  parser.issues().dump(std::cout, parser.src());

  return parser.issues().have_errors() ? 1 : 0;
}
