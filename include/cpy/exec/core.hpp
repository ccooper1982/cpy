#pragma once

#include "cpy/exceptions.hpp"
#include <cpy/common.hpp>

#include <print>
#include <type_traits>
#include <variant>


namespace core
{
  inline VarValue print(const std::span<const VarValue> args)
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

  inline VarValue println(const std::span<const VarValue> args)
  {
    print(args);
    std::println();
    return std::monostate{};
  }

  inline VarValue exit(const std::span<const VarValue> args)
  {
    const int status = args.empty() ? 0 : static_cast<int>(std::get<CpyInt>(args[0]));
    throw ScriptExit {status};
  }
}
