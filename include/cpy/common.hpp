#pragma once
#include <ranges>
#include <filesystem>

namespace fs = std::filesystem;
namespace rg = std::ranges;
namespace vw = std::views;

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
