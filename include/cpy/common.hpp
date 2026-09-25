#pragma once

#include <expected>
#include <filesystem>
#include <ranges>
#include <string_view>

namespace fs = std::filesystem;
namespace rg = std::ranges;
namespace vw = std::views;

// helper type for the visitor
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

struct SourceRegion
{
  SourceRegion() = default;
  SourceRegion(const uint32_t from, const uint32_t to)
    : start(from)
    , end(to)
  {
  }

  uint32_t start{}, end{};
};

struct CpyError
{
  CpyError() = default;

  CpyError(const std::string_view msg) : m_msg(msg)
  {}

  bool operator()() const
  {
    return !m_msg.empty();
  }

  const std::string& msg() const
  {
    return m_msg;
  }

private:
  std::string m_msg;
};

template<typename Expected>
std::expected<Expected, CpyError> make_error(const std::string_view msg)
{
  return std::unexpected(CpyError{msg});
}
