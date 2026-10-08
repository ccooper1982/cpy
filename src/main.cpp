#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>
#include <filesystem>
#include <iostream>

// TODO
//  - separate into libcpy and cpy executable
int main (int argc, char ** argv)
{
  if (argc != 2) {
    return 1;
  }

  fs::path file = argv[1];

  Modules::initialise();

  Parser parser;
  Script script;

  if (!fs::exists(file))
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

  std::cout << "AST:";
  script.ast->dump(std::cout);

  // errors here if tree-sitter found syntax errors
  if (!script.issues->have_errors())
  {
    Semantics sems;
    sems.process(script);

    std::cout << "Sym Table:\n";
    sems.symbol_table().dump(std::cout);
  }

  std::cout << "Issues:\n";
  script.issues->dump(std::cout, *script.src);

  return script.issues->have_errors() ? 1 : 0;
}
