#include "cpy/exceptions.hpp"
#include <cpy/exec/executor.hpp>
#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>
#include <exception>
#include <filesystem>
#include <iostream>


static Parser parser;
static Script script;
static Semantics semantics;

static int execute ();


static bool startup_valid(const int argc, char **& argv)
{
  if (argc != 2)
  {
    std::cerr << "No input source file:\n";
    return false;
  }

  if (const fs::path file = argv[1]; !fs::exists(file))
  {
    std::cerr << "File does not exist: " << file.string() << '\n';
    return false;
  }

  return true;
}

// TODO
//  - separate into libcpy and cpy executable

// return status:
//    0: good
//  < 0: compiler erorr
//  > 0: user error
int main (int argc, char ** argv)
{
  if (!startup_valid(argc, argv)) {
    return 1;
  }

  Modules::initialise();

  try
  {
    const fs::path file = argv[1];
    script = parser.parse(file);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what() << '\n';
    return -1;
  }

  if (script.issues->have_errors())
  {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
    return 1;
  }

  semantics.process(script);

  if (script.issues->have_errors())
  {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
    return 1;
  }

  return execute();
}


static int execute ()
{
  int exit_status{};

  Executor exec {semantics.symbol_table()};

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
