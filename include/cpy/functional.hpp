#pragma once

#include <map>
#include <vector>

#include <cpy/common.hpp>

struct FunctionDef; // forward decl

struct Process
{
  std::string executable;
  std::vector<std::string> args{};

  bool operator==(const Process& p) const
  {
    // TODO incomplete
    return p.executable == executable && p.args.size() ==  args.size();
  };
};


using CpyInt = int64_t;
using CpyDec = double;
using CpyBool = bool;
using CpyString = std::string;

using VarName = std::string_view;
using VarValue = std::variant<std::monostate, CpyString, CpyInt, CpyDec, CpyBool, Process>;


enum class NativeType
{
  Unset,
  Int,
  Decimal,
  Bool,
  String,
  Void,
  Proc
};

inline constexpr const auto unresolved_type = NativeType::Unset;


struct UserType
{
  std::string_view name;
};

struct VarType
{
  VarType() : type (NativeType::Unset)
  {}

  VarType(const NativeType t) : type (t)
  {}

  const auto& value() const
  {
    return type;
  }

  template<typename T>
  const std::optional<T> value_as() const
  {
    if (const auto param_type = std::get_if<T>(&type); param_type)
      return *param_type;
    return std::nullopt;
  }

  template<typename T>
  bool is_type() const requires(std::is_same_v<T, NativeType>)
  {
    return std::holds_alternative<T>(type);
  }

  bool is_type(const NativeType t) const
  {
    return is_type<NativeType>() && *(value_as<NativeType>()) == t;
  }

  explicit operator bool() const
  {
    return !is_type(NativeType::Unset);
  }

private:
  std::variant<NativeType, UserType> type;
};



inline bool operator==([[maybe_unused]] const UserType& a, [[maybe_unused]] const UserType& b)
{
  throw std::runtime_error{"Comparing unsupported UserType"};
}

inline bool operator==(const VarType& a, const VarType& b)
{
  return a.value() == b.value();
}


inline std::string_view to_string(const NativeType t)
{
  switch (t)
  {
    using enum NativeType;

    case Int:
      return "int";
    case Decimal:
      return "dec";
    case Bool:
      return "bool";
    case String:
      return "str";
    case Void:
      return "void";
    case Proc:
      return "Process";
    default:
      return "Unknown";
  }
}


inline std::string_view to_string(const VarType& t)
{
  if (t.is_type<NativeType>()) {
    return to_string(std::get<NativeType>(t.value()));
  }
  else {
     throw std::runtime_error{"UserType not implemented"};
  }
}



struct ResolvedSymbol
{
  std::string_view name;
  VarType type;
  bool is_ref{};
  uint32_t position{};
};

enum class ParamMode
{
  Fixed,
  VariadicAny
};


struct ResolvedFunction
{
  std::vector<ResolvedSymbol> params;
  VarType return_type;
  ParamMode param_mode{ParamMode::Fixed};
  const FunctionDef * ast{};
};

using ResolvedVariableMap = std::map<std::string_view, const ResolvedSymbol>;


struct RuntimeContext
{
  std::vector<std::string> argv;
};



using NativeFunction = VarValue (*)(const RuntimeContext&, const std::span<const VarValue>);

struct NativeFunctionDef
{
  ResolvedFunction signature;
  NativeFunction implementation;
  size_t min_args{};
};


template<auto F>
auto create_function(std::string_view name, std::initializer_list<ResolvedSymbol> params, NativeType return_type, ParamMode param_mode, size_t min_args = 0)
{
  using FunctionEntry = std::pair<std::string_view, NativeFunctionDef>;

  ResolvedFunction func {std::vector<ResolvedSymbol>{params}, return_type, param_mode};

  return FunctionEntry {name, {std::move(func), F, min_args}};
}
