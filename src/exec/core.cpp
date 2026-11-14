#include "shl/functional.hpp"
#include <shl/exec/core.hpp>

#include <shl/common.hpp>
#include <shl/exceptions.hpp>

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <print>
#include <pwd.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>
#include <variant>



namespace core {

VarValue print(const RuntimeContext&, const std::span<const VarValue> args)
{
  for(const auto& val : args)
  {
    std::visit([](const auto& val)
      {
        using T = std::remove_cvref_t<decltype(val)>;

        if constexpr (std::is_same_v<T, Process>) {
          std::print("Executable: {} , Args: {}", val.executable, val.args.size()); // TODO lazy
        }
        else if constexpr (std::is_same_v<T, Pipeline>) {
          std::print("Pipeline: Processes: {} ", val.processes.size());
        }
        else if constexpr (std::is_same_v<T, ExecResult>) {
          std::print("{}", val.stdout);
        }
        else if constexpr (!std::is_same_v<T, std::monostate>) {
          std::print("{}", val);
        }
      },
      val);
  }

  return std::monostate{};
}

VarValue println(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  print(ctx, args);
  std::println();
  return std::monostate{};
}

VarValue exit(const RuntimeContext&, const std::span<const VarValue> args)
{
  const int status = args.empty() ? 0 : static_cast<int>(std::get<CpyInt>(args[0]));

  if (status < 0 || status > 255) {
    throw RuntimeError{"Exit status must be in range [0,255]"};
  }
  throw ScriptExit {status};
}

VarValue home(const RuntimeContext&, const std::span<const VarValue>)
{
  if (const char* home = std::getenv("HOME"); home && *home) {
    return std::string{home};
  }
  else
  {
    passwd pwd;
    passwd * result{};

    std::string buffer;
    buffer.resize(sysconf(_SC_GETPW_R_SIZE_MAX));

    if (const int error = getpwuid_r(getuid(), &pwd, buffer.data(), buffer.size(), &result); error != 0) {
      throw RuntimeError{std::format("getpwuid_r() failed: {}", std::strerror(error))};
    }

    return CpyString{result->pw_dir};
  }
}

VarValue env(const RuntimeContext&, const std::span<const VarValue> args)
{
  const auto * value = std::getenv(std::get<CpyString>(args[0]).data());
  return value ? CpyString{value} : CpyString{""};
}

// conversion //
template<typename T> requires IsCpySimpleType<std::remove_cvref_t<T>>
VarValue value_cast(const VarValue& from)
{
  auto convert = [](const auto& v) -> VarValue
  {
    using From = std::remove_cvref_t<decltype(v)>;
    using To = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<To, From>) {
      return v;
    }
    else if constexpr (IsCpyString<To> && IsCpyArithmetic<From>) {
      return std::to_string(v);
    }
    else if constexpr (IsCpyString<To> && IsCpyBoolean<From>) {
      return v ? "true" : "false";
    }
    else if constexpr (IsCpyArithmetic<To> && IsCpyArithmetic<From>) {
      return static_cast<To>(v);
    }
    else if constexpr (IsCpyBoolean<To> && IsCpyInteger<From>) {
      return v != 0;
    }
    else if constexpr (IsCpyBoolean<To> && IsCpyString<From>) {
      return v == "true" || v == "1";
    }
    else if constexpr (IsCpyInteger<To> && IsCpyString<From>)
    {
      const std::string_view trimmed = trim_ws(v);
      return static_cast<CpyInt>(std::strtoll(trimmed.data(), nullptr, 10));
    }
    else if constexpr (IsCpyDecimal<To> && IsCpyString<From>)
    {
      const std::string_view trimmed = trim_ws(v);
      return std::strtod(trimmed.data(), nullptr);
    }
    else {
      throw RuntimeError{"Invalid value conversion"};
    }
  };

  return std::visit(convert, from);
}

VarValue to_int(const RuntimeContext&, const std::span<const VarValue> args)
{
  return value_cast<CpyInt>(args[0]);
}

VarValue to_dec(const RuntimeContext&, const std::span<const VarValue> args)
{
  return value_cast<CpyDec>(args[0]);
}

VarValue to_bool(const RuntimeContext&, const std::span<const VarValue> args)
{
  return value_cast<CpyBool>(args[0]);
}

VarValue to_str(const RuntimeContext&, const std::span<const VarValue> args)
{
  return value_cast<CpyString>(args[0]);
}

// argv //

bool to_var_value (const std::string_view arg, auto& val)
{
  auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), val);
  return ec == std::errc{} && ptr == arg.data() + arg.size();
}

size_t get_argv_index(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  const auto index = std::get<CpyInt>(args[0]);

  if (index < 0 || static_cast<size_t>(index) >= ctx.argv.size()) {
    throw RuntimeError{std::format("argv index out of range: {}", index)};
  }
  return static_cast<size_t>(index);
}

VarValue get_argv_str(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  const auto index = get_argv_index(ctx, args);
  return ctx.argv[index];
}

VarValue get_argv_int(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  const auto index = get_argv_index(ctx, args);
  if (CpyInt val; to_var_value(ctx.argv[index], val))  {
    return val;
  }

  throw RuntimeError{std::format("argv {} cannot be parsed as an integer", index)};
}

VarValue get_argv_dec(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  const auto index = get_argv_index(ctx, args);
  if (CpyDec val; to_var_value(ctx.argv[index], val))  {
    return val;
  }

  throw RuntimeError{std::format("argv {} cannot be parsed as an decimal", index)};
}

VarValue get_argv_bool(const RuntimeContext& ctx, const std::span<const VarValue> args)
{
  const auto index = get_argv_index(ctx, args);
  const std::string_view val = ctx.argv[index];

  if (val == "true" || val == "1") {
    return true;
  }

  if (val == "false" || val == "0") {
    return false;
  }

  throw RuntimeError{std::format("argv {} cannot be parsed as a boolean", index)};
}

VarValue get_argc(const RuntimeContext& ctx, const std::span<const VarValue>)
{
  return static_cast<CpyInt>(ctx.argv.size());
}

} // ns core
