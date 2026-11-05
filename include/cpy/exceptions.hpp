#pragma once

#include <stdexcept>

class RuntimeError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class ScriptExit : public std::runtime_error
{
public:
  ScriptExit(const int status)
    : runtime_error("")
    , m_status(status)
  {

  }

  int exit_status() const { return m_status; }

private:
  int m_status{};
};
