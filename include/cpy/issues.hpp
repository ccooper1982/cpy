#pragma once

#include <ostream>
#include <vector>

#include <cpy/common.hpp>

struct Issue
{
  Issue(const std::string_view m) : msg(m)
  {
  }

  std::string msg;
  // int_16 line{-1};
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

  bool have_errors() const { return !m_errors.empty(); }

  void dump(std::ostream& os) const
  {
    if (m_errors.empty())
      return;

    os << ColorRed << src_path << " : ERRORS " << ColorReset << '\n';

    for (const auto& err : m_errors)
      os << "-> " << err.msg << '\n';
  }

private:
  fs::path src_path;
  std::vector<Issue> m_errors;
};
