#include "cpy/exceptions.hpp"
#include <cpy/exec/executor.hpp>
#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>
#include <exception>
#include <filesystem>
#include <iostream>

int a{};
void f()
{
  a = 5;
}

// TODO
//  - separate into libcpy and cpy executable
int main (int argc, char ** argv)
{
  if (argc != 2)
  {
    std::cerr << "Need input source file:\n";
    return 1;
  }

  const fs::path file = argv[1];

  if (!fs::exists(file))
  {
    std::cerr << "File does not exist: " << file.string() << '\n';
    return 1;
  }

  Modules::initialise();

  Parser parser;
  Script script;

  try
  {
    script = parser.parse(file);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what() << '\n';
    return -1;
  }

  if (script.issues->have_errors()) {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
  }

  // std::cout << "AST:";
  // script.ast->dump(std::cout);

  Semantics sems;
  sems.process(script);

  // std::cout << "Sym Table:\n";
  // sems.symbol_table().dump(std::cout);


  if (script.issues->have_errors())
  {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
    return 1;
  }

  int exit_status{};

  Executor exec {sems.symbol_table()};

  try
  {
    exec.execute(script.ast->block);
  }
  catch (const ScriptExit& exit)
  {
    exit_status = exit.exit_status();
  }
  catch (const RuntimeError& ex_run)
  {
    std::cerr << "Runtime Error: " << ex_run.what() << '\n';
    return 1;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Internal execution error: " << ex.what() << '\n';
    return -1;
  }
  catch (...)
  {
    std::cerr << "Unknown execution error\n";
    return -1;
  }

  return exit_status;
}
