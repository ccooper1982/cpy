#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include <cpy/common.hpp>



struct Issue
{
  Issue(const std::string_view m) : msg(m)
  {
  }

  Issue(const std::string_view m, const std::optional<SourceRegion>& src)
    : msg(m)
    , src(src)
  {
  }

  Issue(const std::string_view m, const uint32_t from, const uint32_t to)
    : Issue(m, SourceRegion{from,to})
  {
  }

  std::string msg;
  std::optional<SourceRegion> src;
};

class Issues
{
  static constexpr std::string_view ColorRed   = "\033[31m";
  static constexpr std::string_view ColorReset = "\033[0m";

public:
  Issues (const fs::path src) : src_path(src) {}

  void add_error(const std::string_view msg)
  {
    m_errors.emplace_back(msg);
  }

  void add_error(const std::string_view msg, const SourceRegion& sr)
  {
    m_errors.emplace_back(msg, sr);
  }

  void add_error(const std::string_view msg, const uint32_t from, const uint32_t to)
  {
    m_errors.emplace_back(msg, from, to);
  }

  bool have_errors() const { return !m_errors.empty(); }

  void dump(std::ostream& os, std::string_view src) const
  {
    if (m_errors.empty())
      return;

    os << ColorRed << src_path << " : ERRORS " << ColorReset << '\n';

    for (const auto& err : m_errors)
    {
      os << "-> " << err.msg << '\n';
      if (err.src)
        os << std::setw(4) << "> " << src.substr(err.src->start, err.src->end - err.src->start) << '\n';
    }
  }

private:
  fs::path src_path;
  std::vector<Issue> m_errors;
};
