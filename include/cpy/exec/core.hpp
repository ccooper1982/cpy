#pragma once

#include <cpy/common.hpp>



namespace core
{
  VarValue print(const RuntimeContext&, const std::span<const VarValue> args);
  VarValue println(const RuntimeContext& ctx, const std::span<const VarValue> args);
  VarValue exit(const RuntimeContext&, const std::span<const VarValue> args);

  // argv //
  size_t get_argv_index(const RuntimeContext& ctx, const std::span<const VarValue> args);
  VarValue get_argv_str(const RuntimeContext& ctx, const std::span<const VarValue> args);
  VarValue get_argv_int(const RuntimeContext& ctx, const std::span<const VarValue> args);
  VarValue get_argv_dec(const RuntimeContext& ctx, const std::span<const VarValue> args);
  VarValue get_argv_bool(const RuntimeContext& ctx, const std::span<const VarValue> args);
}
