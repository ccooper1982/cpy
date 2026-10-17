#pragma once

#include <functional>
#include <stdexcept>
#include <variant>
#include <cpy/common.hpp>

using VarValue = std::variant<CpyString, CpyInt, CpyDec, CpyBool>;

template<typename T>
concept IsCpyArithmetic = std::is_same_v<T, CpyInt> || std::is_same_v<T, CpyDec>;


template<class OpF>
inline VarValue do_operator (const VarValue& lhs, const VarValue& rhs, OpF&& op)
{
  return std::visit(
      [&](const auto& val_l, const auto& val_r) -> VarValue
      {
        using L = std::decay_t<decltype(val_l)>;
        using R = std::decay_t<decltype(val_r)>;

        if constexpr (IsCpyArithmetic<L> && IsCpyArithmetic<R>) {
          return op(val_l, val_r);
        }
        else {
          throw std::logic_error{"wtf"};
        }
      }
    , lhs, rhs);
}

inline VarValue operator+(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::plus<>{});
}

inline VarValue operator-(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::minus<>{});
}

inline VarValue operator/(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::divides<>{});
}

inline VarValue operator*(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::multiplies<>{});
}

inline VarValue operator<(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::less<>{});
}

inline VarValue operator<=(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::less_equal<>{});
}

inline VarValue operator>(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::greater<>{});
}

inline VarValue operator>=(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::greater_equal<>{});
}

inline VarValue operator==(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::equal_to<>{});
}

inline VarValue operator!=(const VarValue& lhs, const VarValue& rhs)
{
  return do_operator(lhs, rhs, std::not_equal_to<>{});
}
