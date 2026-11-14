#include <cpy/ast/ast_node.hpp>
#include <cpy/common.hpp>
#include <cpy/exceptions.hpp>
#include <cpy/exec/executor.hpp>
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <cpy/semantics.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <unistd.h>


static const constexpr int STATUS_OK = 0;
static const constexpr int STATUS_INTERNAL_ERR = 1;
static const constexpr int STATUS_USER_ERR = 2;

static Parser parser;
static Script script;
static Semantics semantics;


static int execute (std::span<char *> argv);


static bool startup_valid(const int argc, char **& argv)
{
  if (geteuid() == 0)
  {
    std::cerr << "You may regret this, so it's currently disallowed\n";
    return false;
  }

  if (argc < 2)
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


static std::vector<std::string> get_script_args (const std::span<char *> argv)
{
  std::vector<std::string> args;

  // skip exec name and script path
  if (argv.size() > 2) {
    rg::copy(argv.subspan(2), std::back_inserter(args));
  }

  return args;
}

// TODO
//  - separate into libcpy and cpy executable
int main (int argc, char ** argv)
{
  if (!startup_valid(argc, argv)) {
    return STATUS_USER_ERR;
  }

  Modules::initialise();

  try
  {
    script = parser.parse(fs::path{argv[1]});
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what() << '\n';
    return STATUS_INTERNAL_ERR;
  }

  if (script.issues->have_errors())
  {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
    return STATUS_USER_ERR;
  }

  semantics.process(script);

  if (script.issues->have_errors())
  {
    std::cout << "Issues:\n";
    script.issues->dump(std::cout, *script.src);
    return STATUS_USER_ERR;
  }

  return execute(std::span<char *> {argv, static_cast<size_t>(argc)});
}


static int execute (std::span<char *> argv)
{
  int exit_status{STATUS_OK};

  RuntimeContext rt_context {.argv = get_script_args(argv)};

  Executor exec {semantics.symbol_table(), rt_context};

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
    exit_status = STATUS_USER_ERR;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Internal execution error: " << ex.what() << '\n';
    exit_status = STATUS_INTERNAL_ERR;
  }
  catch (...)
  {
    std::cerr << "Unknown execution error\n";
    exit_status = STATUS_INTERNAL_ERR;
  }

  return exit_status;
}
