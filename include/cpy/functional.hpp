#pragma once

#include <map>
#include <vector>

#include <cpy/common.hpp>
#include <cpy/ast/ast.hpp>


struct ResolvedSymbol
{
  std::string_view name;
  VarType type;
  bool is_ref{};
  uint32_t position{};
};

enum class ParamMode
{
  Fixed,
  VariadicAny
};

struct ResolvedFunction
{
  std::vector<ResolvedSymbol> params;
  VarType return_type;
  ParamMode param_mode{ParamMode::Fixed};
  const FunctionDef * ast{};
};

using ResolvedVariableMap = std::map<std::string_view, const ResolvedSymbol>;


struct RuntimeContext
{
  std::vector<std::string> argv;
};
