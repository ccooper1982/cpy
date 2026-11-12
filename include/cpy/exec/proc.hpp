#pragma once

#include <spawn.h>
#include <sys/wait.h>

#include <cpy/exceptions.hpp>
#include <cpy/functional.hpp>



namespace proc {

  struct Pipe
  {
    Pipe () = default;

    Pipe (int read_fd, int write_fd)
      : read(read_fd)
      , write(write_fd)
    {

    }

    ~Pipe ()
    {
      posix_spawn_file_actions_destroy(&actions);
      close(write);
      close(read);
    }

    int read{-1};
    int write{-1};
    posix_spawn_file_actions_t actions;
  };

  inline std::vector<char*> make_command (const Process& proc);
  inline Pipe create_pipe ();
  inline VarValue start_proc (const Process& proc);

  VarValue create (const RuntimeContext&, const std::span<const VarValue> args);
  VarValue exec (const RuntimeContext&, const std::span<const VarValue> args);
  VarValue exit_status (const RuntimeContext&, const std::span<const VarValue> args);

} // ns proc


inline const std::map<std::string_view, NativeFunctionDef> NativeProcFunctions
{
  create_function<proc::create>("proc", {}, NativeType::Process, ParamMode::VariadicAny, 1),
  create_function<proc::exec>("exec", {}, NativeType::ExecResult, ParamMode::VariadicAny, 1),
  create_function<proc::exit_status>("exit_status", {{"result", NativeType::ExecResult}}, NativeType::Int, ParamMode::Fixed)
};
