#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>
#include <type_traits>

#include <shl/common.hpp>

struct FunctionDef; // forward decl

struct Process
{
  std::string executable;
  std::vector<std::string> args{};

  bool operator==(const Process& p) const = default;
};

struct Pipeline
{
    std::vector<Process> processes;

    bool operator==(const Pipeline&) const = default;
};

struct ExecResult
{
  std::string stdout;
  int exit_status{};

  bool operator==(const ExecResult&) const = default;
};

using CpyInt = int64_t;
using CpyDec = double;
using CpyBool = bool;
using CpyString = std::string;

using VarName = std::string_view;
using VarValue = std::variant<std::monostate, CpyString, CpyInt, CpyDec, CpyBool, Process, Pipeline, ExecResult>;


template<typename T1, typename T2, typename CpyType>
concept AreSame = std::is_same_v<T1, CpyType> && std::is_same_v<T2, CpyType> ;

template<typename T>
concept IsCpyDecimal = std::is_same_v<T, CpyDec>;

template<typename T>
concept IsCpyInteger = std::is_same_v<T, CpyInt>;

template<typename T>
concept IsCpyArithmetic = IsCpyInteger<T> || IsCpyDecimal<T>;

template<typename T>
concept IsCpyBoolean = std::is_same_v<T, CpyBool>;

template<typename T>
concept IsCpyString = std::is_same_v<T, CpyString>;

template<typename T>
concept IsCpySimpleType = std::disjunction_v<std::is_same<T, CpyString>, std::is_same<T, CpyInt>, std::is_same<T, CpyDec>, std::is_same<T, CpyBool>>;

template<typename T>
concept IsCpyType = IsCpySimpleType<T> || std::disjunction_v<std::is_same<T, Process>, std::is_same<T, Pipeline>, std::is_same<T, ExecResult>>;

template<typename T1, typename T2>
concept AreCpyInteger = AreSame<T1, T2, CpyInt>;

template<typename T1, typename T2>
concept AreCpyDecimal = AreSame<T1, T2, CpyDec>;

template<typename T1, typename T2>
concept AreCpyString = AreSame<T1, T2, CpyString>;

template<typename T1, typename T2>
concept AreCpyArithmetic = IsCpyArithmetic<T1> && IsCpyArithmetic<T2> ;



enum class NativeType
{
  Unset,
  Int,
  Decimal,
  Bool,
  String,
  Void,
  Process,
  Pipeline,
  ExecResult
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
    case Process:
      return "Process";
    case Pipeline:
      return "Pipeline";
    case ExecResult:
      return "ExecResult";
    case Unset:
      return "Unset";
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



inline std::string to_string(const VarValue& v)
{
  return std::visit([](const auto val) -> std::string
  {
    using T = std::remove_cvref_t<decltype(val)>;

    if constexpr (IsCpyBoolean<T>) {
      return val ? "1" : "0";
    }
    else if constexpr (IsCpyArithmetic<T>) {
      return std::to_string(val);
    }
    else if constexpr (IsCpyString<T>) {
      return val;
    }
    else {
      throw std::runtime_error{"to_string(const VarValue&) called with invalid type"};
    }
  },
  v);
}

template<typename T>
VarValue make_value (T v)
{
  if constexpr (std::is_same_v<T, CpyBool>) {
    return CpyBool(v);
  }
  else if constexpr (std::is_same_v<T, Process>) {
    return Process{};
  }
  else if constexpr (std::is_integral_v<T>) {
    return CpyInt(v);
  }
  else if constexpr (std::floating_point<T>) {
    return CpyDec(v);
  }
  else if constexpr (std::is_convertible_v<T, CpyString>) {
    return CpyString(v);
  }
  else {
    static_assert(false, "Unsupported VarValue type");
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
