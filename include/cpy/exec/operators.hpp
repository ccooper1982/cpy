#pragma once

#include <cmath>
#include <functional>
#include <variant>
#include <cpy/common.hpp>
#include <cpy/exceptions.hpp>

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

template<typename T1, typename T2>
concept AreCpyInteger = IsCpyInteger<T1> && IsCpyInteger<T2>;

template<typename T1, typename T2>
concept AreCpyDecimal = IsCpyDecimal<T1> && IsCpyDecimal<T2>;

template<typename T1, typename T2>
concept AreCpyArithmetic = IsCpyArithmetic<T1> && IsCpyArithmetic<T2> ;


// binary //

template<class OpF>
constexpr VarValue do_binary_arithmetic (auto& val_l, auto& val_r, OpF op)
{
  using L = std::decay_t<decltype(val_l)>;
  using R = std::decay_t<decltype(val_r)>;

  if constexpr (std::is_same_v<OpF, std::divides<>> && AreCpyInteger<L,R>)
  {
    if (val_r == 0) {
      throw RuntimeError{"Divide by 0"};
    }

    if (val_l == std::numeric_limits<CpyInt>::min() && val_r == -1) {
      throw RuntimeError{"Integer division overflow"};
    }
  }
  else if constexpr (std::is_same_v<OpF, std::plus<>> && AreCpyInteger<L,R>)
  {
    L result;
    if (__builtin_add_overflow(val_l, val_r, &result)) {
      throw RuntimeError{"Integer addition overflow"};
    }
    return result;
  }
  else if constexpr (std::is_same_v<OpF, std::minus<>> && AreCpyInteger<L, R>)
  {
    L result;
    if (__builtin_sub_overflow(val_l, val_r, &result))
        throw RuntimeError{"Integer subtraction overflow"};

    return result;
  }
  else if constexpr (std::is_same_v<OpF, std::multiplies<>>)
  {
    if constexpr (AreCpyInteger<L,R>)
    {
      L result;
      if (__builtin_mul_overflow(val_l, val_r, &result)) {
        throw RuntimeError{"Integer multiplication overflow"};
      }
      return result;
    }
    else if constexpr (AreCpyDecimal<L, R>)
    {
      auto res = op(val_l, val_r);
      if (!std::isfinite(res)) {
        throw RuntimeError{"Invalid floating-point result"};
      }
      return res;
    }
  }
  return op(val_l, val_r);
}


template<class OpF>
inline VarValue do_binary_operator (const VarValue& lhs, const VarValue& rhs, OpF op)
{
  auto handle = [&](const auto& val_l, const auto& val_r) -> VarValue
  {
    using L = std::decay_t<decltype(val_l)>;
    using R = std::decay_t<decltype(val_r)>;

    constexpr bool is_arithmetic = AreCpyArithmetic<L,R>;
    constexpr bool is_boolean = IsCpyBoolean<L> && IsCpyBoolean<R>;
    constexpr bool is_strings = IsCpyString<L> && IsCpyString<R> && (std::is_same_v<OpF, std::plus<>> || std::is_same_v<OpF, std::equal_to<>>);

    if constexpr (!(is_arithmetic || is_strings || is_boolean)) {
      throw std::logic_error{"Called do_binary_operator() on unsupported type"};
    }
    else if constexpr (is_boolean) {
      return op(val_l, val_r);
    }
    else if constexpr (is_arithmetic) {
      return do_binary_arithmetic(val_l, val_r, op);
    }
    else {
      return op(val_l, val_r); // string
    }
  };

  return std::visit(handle, lhs, rhs);
}

inline VarValue operator+(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::plus<>{});
}

inline VarValue operator-(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::minus<>{});
}

inline VarValue operator/(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::divides<>{});
}

inline VarValue operator*(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::multiplies<>{});
}

inline VarValue operator<(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::less<>{});
}

inline VarValue operator<=(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::less_equal<>{});
}

inline VarValue operator>(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::greater<>{});
}

inline VarValue operator>=(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::greater_equal<>{});
}

inline VarValue operator==(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::equal_to<>{});
}

inline VarValue operator!=(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::not_equal_to<>{});
}

inline VarValue operator&&(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::logical_and<>{});
}

inline VarValue operator||(const VarValue& lhs, const VarValue& rhs)
{
  return do_binary_operator(lhs, rhs, std::logical_or<>{});
}

// unary //

template<class OpF>
inline VarValue do_unary_operator (const VarValue& lhs, OpF op)
{
  auto handle = [&](const auto& val) -> VarValue
  {
    using L = std::decay_t<decltype(val)>;

    // Negate: Dec and Int
    // Not: Bool
    // BitwiseNot: Int
    constexpr bool valid =  (std::is_same_v<OpF, std::negate<>> && IsCpyArithmetic<L>) ||
                            (std::is_same_v<OpF, std::logical_not<>> && IsCpyBoolean<L>) ||
                            (std::is_same_v<OpF, std::bit_not<>> && IsCpyInteger<L>);

    if constexpr (valid) {
      return op(val);
    }
    else {
      throw std::logic_error{"Called do_unary_operator() on unsupported type"};
    }
  };

  return std::visit(handle, lhs);
}

inline VarValue operator!(const VarValue& lhs)
{
  return do_unary_operator(lhs, std::logical_not<>{});
}

inline VarValue operator-(const VarValue& lhs)
{
  return do_unary_operator(lhs, std::negate<>{});
}

inline VarValue operator~(const VarValue& lhs)
{
  return do_unary_operator(lhs, std::bit_not<>{});
}
