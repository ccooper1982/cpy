#pragma once

#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/ast/ast_node.hpp>

class Semantics
{
public:
  Semantics(SourceFile& root, Issues& issues, std::string_view src)
    : root(root)
    , issues(issues)
    , src(src)
  {}

  void process();

private:
  SourceFile& root;
  Issues& issues;
  std::string_view src;
};
