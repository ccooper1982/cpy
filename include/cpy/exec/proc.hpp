#pragma once

#include <spawn.h>
#include <sys/wait.h>

#include <cpy/exceptions.hpp>
#include <cpy/functional.hpp>



namespace proc {

  struct Pipe
  {
    Pipe (const int read_fd = -1, const int write_fd = -1)
      : read(read_fd)
      , write(write_fd)
    {

    }

    int read{-1};
    int write{-1};
  };

  VarValue create_proc (const RuntimeContext&, const std::span<const VarValue> args);
  VarValue exec (const RuntimeContext&, const std::span<const VarValue> args);
  VarValue exit_status (const RuntimeContext&, const std::span<const VarValue> args);

} // ns proc


inline const std::map<std::string_view, NativeFunctionDef> NativeProcFunctions
{
  create_function<proc::create_proc>("proc", {}, NativeType::Process, ParamMode::VariadicAny, 1),
  create_function<proc::exec>("exec", {}, NativeType::ExecResult, ParamMode::VariadicAny, 1),
  create_function<proc::exit_status>("exit_status", {{"result", NativeType::ExecResult}}, NativeType::Int, ParamMode::Fixed)
};
