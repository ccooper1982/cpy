#pragma once
#include <ranges>
#include <filesystem>

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
