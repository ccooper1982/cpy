#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>
#include <filesystem>

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
  Script script;

  if (file.empty()) {
    script = parser.parse(src);
  }
  else if (!fs::exists(file))
  {
    std::cerr << "File does not exist: " << file.string() << '\n';
    return 1;
  }
  else
  {
    try
    {
      script = parser.parse(file);
    }
    catch (const std::exception& ex)
    {
      std::cerr << ex.what() << '\n';
      return 1;
    }
  }

  script.ast->dump(std::cout);

  if (!script.issues->have_errors())
  {
    Semantics sems;
    sems.process(script);
  }

  script.issues->dump(std::cout, script.src);

  return script.issues->have_errors() ? 1 : 0;
}
