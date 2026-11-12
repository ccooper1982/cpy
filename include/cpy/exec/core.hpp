#pragma once

#include <cpy/common.hpp>
#include <cpy/functional.hpp>
#include <initializer_list>


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
  VarValue get_argc(const RuntimeContext& ctx, const std::span<const VarValue> args);
}


inline const std::map<std::string_view, NativeFunctionDef> NativeCoreFunctions
{
  create_function<core::print>("print", {}, NativeType::Void, ParamMode::VariadicAny),
  create_function<core::println>("println", {}, NativeType::Void, ParamMode::VariadicAny),
  create_function<core::exit>("exit", {{"status", NativeType::Int}},NativeType::Void, ParamMode::Fixed),
  // argv
  create_function<core::get_argc>("arg_count", {}, NativeType::Int, ParamMode::Fixed),
  create_function<core::get_argv_str>("arg_str", {{"index", NativeType::Int}}, NativeType::String, ParamMode::Fixed),
  create_function<core::get_argv_int>("arg_int", {{"index", NativeType::Int}}, NativeType::Int, ParamMode::Fixed),
  create_function<core::get_argv_dec>("arg_dec", {{"index", NativeType::Int}}, NativeType::Decimal, ParamMode::Fixed),
  create_function<core::get_argv_bool>("arg_bool", {{"index", NativeType::Int}}, NativeType::Bool, ParamMode::Fixed)
};
