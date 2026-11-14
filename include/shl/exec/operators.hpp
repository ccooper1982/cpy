#pragma once

#include <cmath>
#include <functional>
#include <stdexcept>
#include <variant>
#include <shl/functional.hpp>
#include <shl/exceptions.hpp>


// binary //

template<class OpF>
constexpr VarValue do_binary_arithmetic (auto& val_l, auto& val_r, OpF op)
{
  using L = std::decay_t<decltype(val_l)>;
  using R = std::decay_t<decltype(val_r)>;

  if constexpr (std::is_same_v<OpF, std::divides<>> && AreShlInteger<L,R>)
  {
    if (val_r == 0) {
      throw RuntimeError{"Divide by 0"};
    }

    if (val_l == std::numeric_limits<ShlInt>::min() && val_r == -1) {
      throw RuntimeError{"Integer division overflow"};
    }
  }
  else if constexpr (std::is_same_v<OpF, std::plus<>> && AreShlInteger<L,R>)
  {
    L result;
    if (__builtin_add_overflow(val_l, val_r, &result)) {
      throw RuntimeError{"Integer addition overflow"};
    }
    return result;
  }
  else if constexpr (std::is_same_v<OpF, std::minus<>> && AreShlInteger<L, R>)
  {
    L result;
    if (__builtin_sub_overflow(val_l, val_r, &result))
        throw RuntimeError{"Integer subtraction overflow"};

    return result;
  }
  else if constexpr (std::is_same_v<OpF, std::multiplies<>>)
  {
    if constexpr (AreShlInteger<L,R>)
    {
      L result;
      if (__builtin_mul_overflow(val_l, val_r, &result)) {
        throw RuntimeError{"Integer multiplication overflow"};
      }
      return result;
    }
    else if constexpr (AreShlDecimal<L, R>)
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

    // not all combinations are valid C++, and some are valid C++ but invalid shl
    // if invalid shl, the semantic checker will raise an issue, preventing execution
    constexpr bool is_arithmetic = AreShlArithmetic<L,R>;
    constexpr bool is_boolean = AreSame<L,R,ShlBool>;
    constexpr bool is_strings = AreSame<L,R,ShlString> && (std::is_same_v<OpF, std::plus<>> || std::is_same_v<OpF, std::equal_to<>>);


    if constexpr (!(is_arithmetic || is_strings || is_boolean)) {
      throw std::logic_error{"Called do_binary_operator() on unsupported type"};
    }
    else if constexpr (is_arithmetic) {
      return do_binary_arithmetic(val_l, val_r, op);
    }
    else {
      return op(val_l, val_r);
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

inline bool operator<(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::less<>{});
  return std::get<ShlBool>(result);
}

// binary comparison //

inline bool operator<=(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::less_equal<>{});
  return std::get<ShlBool>(result);
}

inline bool operator>(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::greater<>{});
  return std::get<ShlBool>(result);
}

inline bool operator>=(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::greater_equal<>{});
  return std::get<ShlBool>(result);
}

inline bool operator==(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::equal_to<>{});
  return std::get<ShlBool>(result);
}

inline bool operator!=(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::not_equal_to<>{});
  return std::get<ShlBool>(result);
}

// logical //

inline bool operator&&(const VarValue& lhs, const VarValue& rhs)
{
  const auto result = do_binary_operator(lhs, rhs, std::logical_and<>{});
  return std::get<ShlBool>(result);
}

inline bool operator||(const VarValue& lhs, const VarValue& rhs)
{
  const auto result =  do_binary_operator(lhs, rhs, std::logical_or<>{});
  return std::get<ShlBool>(result);
}

// pipes //

inline VarValue operator|(const VarValue& lhs, const VarValue& rhs)
{
  auto handle = overloads
  {
    [&](const Process& left, const Process& right) -> VarValue
    {
      Pipeline pipe;
      pipe.processes = {left, right};
      return pipe;
    },
    [&](const Pipeline& left, const Process& right) -> VarValue
    {
      Pipeline result = left;
      result.processes.emplace_back(right);
      return result;
    },
    [&](const Process& left, const Pipeline& right) -> VarValue
    {
      Pipeline result {right};
      result.processes.insert(result.processes.begin(), left);
      return result;
    },
    [&](const Pipeline& left, const Pipeline& right) -> VarValue
    {
        Pipeline result {left};
        result.processes.insert_range(result.processes.end(), right.processes);
        return result;
    },
    [&](const auto&, const auto&) -> VarValue
    {
      throw std::logic_error{"Called do_pipes_operator() on unsupported type"};
    }
  };

  return std::visit(handle, lhs, rhs);
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
    constexpr bool valid =  (std::is_same_v<OpF, std::negate<>> && IsShlArithmetic<L>) ||
                            (std::is_same_v<OpF, std::logical_not<>> && IsShlBoolean<L>) ||
                            (std::is_same_v<OpF, std::bit_not<>> && IsShlInteger<L>);

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
