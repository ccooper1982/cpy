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
#include <string_view>


static Parser parser;
static Script script;
static Semantics semantics;

static int execute (std::span<char *> argv);


static bool startup_valid(const int argc, char **& argv)
{
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

// static std::vector<VarValue> get_args (const std::span<char *> argv)
// {
//   auto to_var_value = [](const std::string_view arg, auto& val) -> bool
//   {
//     auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), val);
//     return ec == std::errc{} && ptr == arg.data() + arg.size();
//   };
//
//   std::vector<VarValue> args;
//
//   if (argv.size() > 2)
//   {
//     args.reserve(argv.subspan(2).size());
//
//     for (const std::string_view arg : argv.subspan(2))
//     {
//       if (arg == "true" || arg == "false") {
//         args.emplace_back(CpyBool{arg == "true"});
//       }
//       else if (CpyInt integer{}; to_var_value(arg, integer)) {
//         args.emplace_back(integer);
//       }
//       else if (CpyDec decimal{}; to_var_value(arg, decimal)) {
//         args.emplace_back(decimal);
//       }
//       else {
//         args.emplace_back(CpyString{arg});
//       }
//     }
//   }
//
//   return args;
// }

static std::vector<std::string> get_args (const std::span<char *> argv)
{
  std::vector<std::string> args;

  if (argv.size() > 2)
  {
    for (const std::string_view arg : argv.subspan(2)){
      args.emplace_back(arg);
    }
  }

  return args;
}


// TODO
//  - separate into libcpy and cpy executable

// return status:
//    0: good
//  < 0: compiler error
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

  return execute(std::span<char *> {argv, static_cast<size_t>(argc)});
}


static int execute (std::span<char *> argv)
{
  int exit_status{};

  RuntimeContext rt_context {.argv =get_args(argv)};

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
