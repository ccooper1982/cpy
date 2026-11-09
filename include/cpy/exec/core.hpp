#pragma once

#include <cpy/common.hpp>
#include <cpy/functional.hpp>


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


using CoreFunction = VarValue (*)(const RuntimeContext&, const std::span<const VarValue>);

struct CoreFunctionDef
{
  ResolvedFunction signature;
  CoreFunction implementation;
  size_t min_args{};
};

inline const std::map<std::string_view, CoreFunctionDef> CoreFunctions {
  {
    "print",
    {
      {
        {},
        BuiltInType::Void,
        ParamMode::VariadicAny
      },
      core::print
    }
  },
  {
    "println",
    {
      {
        {},
        BuiltInType::Void,
        ParamMode::VariadicAny
      },
      core::println
    }
  },
  {
    "exit",
    {
      {
        {
          {"status", BuiltInType::Int}
        },
        BuiltInType::Void,
        ParamMode::Fixed
      },
      core::exit
    }
  },
  {
    "arg_count",
    {
      {
        {{}},
        BuiltInType::Int,
        ParamMode::Fixed
      },
      core::get_argc
    }
  },
  {
    "arg_str",
    {
      {
        {
          {"index", BuiltInType::Int}
        },
        BuiltInType::String,
        ParamMode::Fixed
      },
      core::get_argv_str
    }
  },
  {
    "arg_int",
    {
      {
        {
          {"index", BuiltInType::Int}
        },
        BuiltInType::Int,
        ParamMode::Fixed
      },
      core::get_argv_int
    }
  },
  {
    "arg_dec",
    {
      {
        {
          {"index", BuiltInType::Int}
        },
        BuiltInType::Decimal,
        ParamMode::Fixed
      },
      core::get_argv_dec
    }
  },
  {
    "arg_bool",
    {
      {
        {
          {"index", BuiltInType::Int}
        },
        BuiltInType::Bool,
        ParamMode::Fixed
      },
      core::get_argv_bool
    }
  }
};
