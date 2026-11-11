#pragma once

#include <cpy/exceptions.hpp>
#include <cpy/functional.hpp>

namespace proc {

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

} // ns proc


inline const std::map<std::string_view, NativeFunctionDef> NativeProcFunctions
{
  create_function<proc::create>("proc", {}, NativeType::Process, ParamMode::VariadicAny, 1)
};
