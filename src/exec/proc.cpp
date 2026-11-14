
#include "cpy/exceptions.hpp"
#include <cpy/functional.hpp>
#include <cstdio>
#include <cstring>
#include <exception>
#include <print>
#include <stdexcept>
#include <sys/types.h>
#include <cpy/exec/proc.hpp>
#include <unistd.h>


extern "C" {
  extern char** environ;
}


namespace proc {

  static std::vector<char*> make_command (const Process& proc)
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

  static std::vector<pid_t> start_processes(const std::vector<Process> processes, const std::vector<Pipe>& pipes)
  {
    std::vector<pid_t> pids;
    pids.reserve(processes.size());

    for (size_t i = 0 ; i < processes.size(); ++i)
    {
      posix_spawn_file_actions_t actions;
      posix_spawn_file_actions_init(&actions);

      posix_spawn_file_actions_adddup2(&actions, pipes[i].write, STDOUT_FILENO);

      if (i > 0) {
        posix_spawn_file_actions_adddup2(&actions, pipes[i-1].read, STDIN_FILENO);
      }

      for (const auto& p : pipes)
      {
        posix_spawn_file_actions_addclose(&actions, p.read);
        posix_spawn_file_actions_addclose(&actions, p.write);
      }

      const auto& proc = processes[i];
      auto cmd = make_command(proc);
      pid_t pid{};

      const int error = posix_spawnp(&pid, proc.executable.c_str(), &actions, nullptr, cmd.data(),::environ);

      posix_spawn_file_actions_destroy(&actions);

      if (error != 0) {
        throw RuntimeError{std::format("spawn of {} failed: {}", proc.executable, std::strerror(errno))};
      }

      pids.push_back(pid);
    }

    return pids;
  }

  static int wait_for_process (const pid_t pid)
  {
    int status{};
    while (waitpid(pid, &status, 0) == -1)
    {
      if (errno == EINTR) {
        continue;
      }

      throw RuntimeError{std::format("start_proc(): waitpid failed: {}", std::string(std::strerror(errno)))};
    }
    return status;
  }

  static VarValue exec_pipeline (const Pipeline& pl)
  {
    std::vector<Pipe> pipes;
    pipes.reserve(pl.processes.size());

    for (size_t i = 0 ; i < pl.processes.size(); ++i)
    {
      int fds[2];

      if (pipe(fds) == -1) {
        throw RuntimeError{"pipe() failed"};
      }
      pipes.emplace_back(fds[0], fds[1]);
    }

    const std::vector<pid_t> pids = start_processes(pl.processes, pipes);

    for (size_t i = 0; i < pipes.size(); ++i)
    {
      close(pipes[i].write);

      if (i + 1 < pipes.size()) {
        close(pipes[i].read);
      }
    }

    ExecResult result;
    char buffer[4096];

    while (true)
    {
      if (const auto n = read(pipes.back().read, buffer, sizeof(buffer)); n > 0) {
        result.stdout.append(buffer, static_cast<size_t>(n));
      }
      else if (n == 0) {
        break;
      }
      else if (errno != EINTR) {
        break;
      }
    }

    close(pipes.back().read);

    int exit_status{}; // exit status of the last process
    for (const auto& pid : pids)
    {
      try {
        exit_status = wait_for_process(pid);
      }
      catch (const std::exception& ex){
        std::println(stderr, "{}", ex.what());
      }
    }

    if (WIFEXITED(exit_status))
    {
      result.exit_status = WEXITSTATUS(exit_status);
      return result;
    }

    if (WIFSIGNALED(exit_status))
    {
      result.exit_status = 128 + WTERMSIG(exit_status);
      return result;
    }

    throw std::runtime_error{"start_proc(): Unexpected process termination"};
  }

  VarValue create_proc (const RuntimeContext&, const std::span<const VarValue> args)
  {
    Process proc {.executable = std::get<CpyString>(args[0])};

    if (proc.executable.empty()) {
      throw RuntimeError{"Cannot have empty executable in proc()"};
    }

    for (const auto& arg : args.subspan(1)) {
      proc.args.emplace_back(to_string(arg));
    }

    return proc;
  }

  VarValue exec (const RuntimeContext&, const std::span<const VarValue> args)
  {
    if (const auto * proc = std::get_if<Process>(&args[0]); proc)
    {
      Pipeline pipeline = {{*proc}};
      return exec_pipeline(pipeline);
    }
    else if (const auto * pipeline = std::get_if<Pipeline>(&args[0]); pipeline) {
      return exec_pipeline(*pipeline);
    }
    else {
      throw std::runtime_error{"exec() called with incompatible type"};
    }
  }

  VarValue exit_status (const RuntimeContext&, const std::span<const VarValue> args)
  {
    return static_cast<CpyInt>(std::get<ExecResult>(args[0]).exit_status);
  }

  VarValue exec_stdout (const RuntimeContext&, const std::span<const VarValue> args)
  {
    return std::get<ExecResult>(args[0]).stdout; // TODO this copies
  }

} // ns proc
