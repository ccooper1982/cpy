
#include "cpy/common.hpp"
#include "cpy/functional.hpp"
#include <cstring>
#include <sys/types.h>
#include <cpy/exec/proc.hpp>


extern "C" {
  extern char** environ;
}


namespace proc {

  std::vector<char*> make_command (const Process& proc)
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


  Pipe create_pipe ()
  {
    /*
    * Before dup2():
    *   FD new ──────────> stdout
    *   FD old ──────────> Pipe (write end)
    *
    * After dup2():
    *   FD new ─────┐
    *               ├────> Pipe (write end)
    *   FD old ─────┘
    *
    * After close():
    *   FD 1 ──────────> Pipe (write end)
    *   FD 4             Closed
    */

    int fds[2];

    if (pipe(fds) == -1) {
      throw std::runtime_error{"pipe() failed"};
    }

    Pipe pipe {fds[0], fds[1]};

    // posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&pipe.actions);

    // Redirect child's stdout to the pipe's input
    posix_spawn_file_actions_adddup2(&pipe.actions, pipe.write, STDOUT_FILENO);

    // Child doesn't need the read end.
    posix_spawn_file_actions_addclose(&pipe.actions, pipe.read);
    // Now stdout refers to pipe's write, we don't need it
    posix_spawn_file_actions_addclose(&pipe.actions, pipe.write);

    return pipe;
  }


  VarValue start_proc (const Process& proc)
  {
    auto cmd = make_command(proc);

    pid_t pid{};

    Pipe pipe = create_pipe();

    const int error = posix_spawnp(&pid, proc.executable.data(), &pipe.actions, nullptr, cmd.data(), environ);

    // close FD to the new pipe since we only need the redirected FD (stdout)
    close(pipe.write);

    if (error != 0) {
      throw RuntimeError{std::format("start_proc(): Failed to launch {}: {}", proc.executable, std::strerror(error))};
    }


    // read
    ExecResult result;
    char buffer[4096];

    while (true)
    {
      if (const auto n = read(pipe.read, buffer, sizeof(buffer)); n > 0) {
        result.stdout.append(buffer, static_cast<size_t>(n));
      }
      else if (n == 0) {
        break;
      }
      else if (errno != EINTR) {
        break;
      }
    }

    close(pipe.read);

    int status{};
    while (waitpid(pid, &status, 0) == -1)
    {
      if (errno == EINTR) {
        continue;
      }

      throw RuntimeError{std::format("start_proc(): waitpid failed: {}", std::string(std::strerror(errno)))};
    }

    if (WIFEXITED(status))
    {
      result.exit_status = WEXITSTATUS(status);
      return result;
    }

    if (WIFSIGNALED(status))
    {
      result.exit_status = 128 + WTERMSIG(status);
      return result;
    }

    throw RuntimeError{"start_proc(): Unexpected process termination"};
  }


  VarValue create (const RuntimeContext&, const std::span<const VarValue> args)
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

  VarValue exec (const RuntimeContext&, const std::span<const VarValue> args)
  {
    const auto& proc = std::get<Process>(args[0]);

    return start_proc(proc);
  }

  VarValue exit_status (const RuntimeContext&, const std::span<const VarValue> args)
  {
    return static_cast<CpyInt>(std::get<ExecResult>(args[0]).exit_status);
  }

} // ns proc
