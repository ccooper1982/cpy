#pragma once

#include <cstring>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cpy/exceptions.hpp>
#include <cpy/functional.hpp>

extern "C" {
  extern char** environ;
}


namespace proc {

inline std::vector<char*> make_command (const Process& proc)
{
  std::vector<char*> argv;
  argv.reserve(proc.args.size() + 2); // executable + nullptr

  argv.push_back(const_cast<char*>(proc.executable.c_str()));

  for (const auto& arg : proc.args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
  }

  argv.push_back(nullptr);
  return argv;
}

inline int start_proc (const Process& proc)
{
  auto cmd = make_command(proc);

  pid_t pid{};
  const int error = posix_spawnp(&pid, proc.executable.data(), nullptr, nullptr, cmd.data(), environ);

  if (error != 0) {
    throw RuntimeError{std::format("Failed to launch {}: {}", proc.executable, std::strerror(error))};
  }

  int status{};
  while (waitpid(pid, &status, 0) == -1)
  {
    if (errno == EINTR) {
      continue;
    }

    throw std::runtime_error{"waitpid failed: " + std::string(std::strerror(errno))};
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }

  throw std::runtime_error{"Unexpected process termination"};
}


inline VarValue create (const RuntimeContext&, const std::span<const VarValue> args)
{
  Process proc {.executable = std::get<CpyString>(args[0])};

  // TODO prefer not to throw, but may be the only option
  if (proc.executable.empty()) {
    throw RuntimeError{"Cannot have empty executable in proc()"};
  }

  for (const auto& arg : args.subspan(1)) {
    proc.args.emplace_back(std::get<CpyString>(arg));
  }

  return proc;
}

inline VarValue proc_run (const RuntimeContext&, const std::span<const VarValue> args)
{
  const auto& proc = std::get<Process>(args[0]);

  return start_proc(proc);
}

} // ns proc


inline const std::map<std::string_view, NativeFunctionDef> NativeProcFunctions
{
  create_function<proc::create>("proc", {}, NativeType::Process, ParamMode::VariadicAny, 1),
  create_function<proc::proc_run>("proc_run", {{"process", NativeType::Process}}, NativeType::Int, ParamMode::Fixed)
};
