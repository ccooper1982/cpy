#pragma once

#include "cpy/ast/ast_node.hpp"
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


using NativeFunction = VarValue (*)(const RuntimeContext&, const std::span<const VarValue>);

struct NativeFunctionDef
{
  ResolvedFunction signature;
  NativeFunction implementation;
  size_t min_args{};
};


template<auto F>
auto create_function(std::string_view name, std::initializer_list<ResolvedSymbol> params, BuiltInType return_type, ParamMode param_mode)
{
  using FunctionEntry = std::pair<std::string_view, NativeFunctionDef>;

  ResolvedFunction func {std::vector<ResolvedSymbol>{params}, return_type, param_mode};

  return FunctionEntry {name, {std::move(func), F}};
}

inline const std::map<std::string_view, NativeFunctionDef> NativeFunctions
{
  create_function<core::print>("print", {}, BuiltInType::Void, ParamMode::VariadicAny),
  create_function<core::println>("println", {}, BuiltInType::Void, ParamMode::VariadicAny),
  create_function<core::exit>("exit", {{"status", BuiltInType::Int}},BuiltInType::Void, ParamMode::Fixed),
  // argv
  create_function<core::get_argc>("arg_count", {}, BuiltInType::Int, ParamMode::Fixed),
  create_function<core::get_argv_str>("arg_str", {{"index", BuiltInType::Int}}, BuiltInType::String, ParamMode::Fixed),
  create_function<core::get_argv_int>("arg_int", {{"index", BuiltInType::Int}}, BuiltInType::Int, ParamMode::Fixed),
  create_function<core::get_argv_dec>("arg_dec", {{"index", BuiltInType::Int}}, BuiltInType::Decimal, ParamMode::Fixed),
  create_function<core::get_argv_bool>("arg_bool", {{"index", BuiltInType::Int}}, BuiltInType::Bool, ParamMode::Fixed)
};
