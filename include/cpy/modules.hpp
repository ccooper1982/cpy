#pragma once

#include <algorithm>
#include <functional>
#include <string_view>
#include <vector>
#include <cpy/common.hpp>


class Module
{
public:
  Module(const std::string_view name) : m_name(name)
  {

  }

  const std::string& name() const { return m_name;}

private:
  std::string m_name;
};

class Modules
{
public:
  static void initialise()
  {
    m_modules.emplace_back("file");

    rg::sort(m_modules, std::less<>{}, &Module::name);
  }

  static bool exist(const std::string_view name)
  {
    return rg::binary_search(m_modules, name, std::less<>{}, &Module::name);
  }

private:
  inline static std::vector<Module> m_modules;
};
