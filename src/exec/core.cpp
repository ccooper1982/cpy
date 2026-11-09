#include <cpy/exec/core.hpp>

#include <cpy/common.hpp>
#include <cpy/exceptions.hpp>

#include <print>
#include <string_view>
#include <type_traits>
#include <variant>



namespace core {

VarValue print(const RuntimeContext&, const std::span<const VarValue> args)
{
  for(const auto& val : args)
  {
    std::visit([](const auto& val)
      {
        using T = std::remove_cvref_t<decltype(val)>;

        if constexpr (!std::is_same_v<T, std::monostate>) {
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
