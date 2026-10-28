#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <cpy/ast/ast_node.hpp>

#include "tree_sitter/api.h"

enum class ErrorCode
{
  ModuleNotExist,
  FunctionDuplicate,
  FunctionNotExist,
  FunctionCallArgType,
  FunctionCallArgVoid,
  FunctionCallArgsCount,
  FunctionCallRefRequired,
  FunctionCallParamNotRef,
  FunctionCallParamRefTypeInvalid,
  FunctionReturnExpression,
  FunctionReturnExprExpected,
  UnknownParamType,
  UnknownReturnType,
  UnknownVarType,
  VariableDuplicate,
  VariableInitVoid,
  VariableInitType,
  VariableUnknown,
  VariableAssignType,
  BinaryOperandsInvalid,
  BinaryOperandsVoid,
  ConditionNotBool,
  ReturnFromRoot,
  Unsupported,
  SyntaxError,
  Compiler
};

struct Issue
{
  Issue(const std::string_view m, const ErrorCode ec) : msg(m), code(ec)
  {
  }

  Issue(const std::string_view m, const std::optional<SourceRegion>& src, const ErrorCode ec)
    : msg(m)
    , src(src)
    , code(ec)
  {
  }

  Issue(const std::string_view m, const uint32_t from, const uint32_t to, const ErrorCode ec)
    : Issue(m, SourceRegion{from,to}, ec)
  {
  }

  std::string msg;
  std::optional<SourceRegion> src;
  ErrorCode code;
};

class Issues
{
  static constexpr std::string_view ColorRed   = "\033[31m";
  static constexpr std::string_view ColorReset = "\033[0m";

public:
  Issues() = default;
  Issues(const fs::path src_file) : src_path(src_file)
  {}

  void add_error(const std::string_view msg, const ErrorCode ec)
  {
    m_errors.emplace_back(msg, ec);
  }

  void add_error(const std::string_view msg, const SourceRegion& sr, const ErrorCode ec)
  {
    m_errors.emplace_back(msg, sr, ec);
  }

  void add_error(const std::string_view msg, const uint32_t from, const uint32_t to, const ErrorCode ec)
  {
    m_errors.emplace_back(msg, from, to, ec);
  }

  bool have_errors() const { return !m_errors.empty(); }

  size_t get_count(const ErrorCode ec) const
  {
    return rg::count(m_errors, ec, &Issue::code);
  }

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

// TODO review these:
// - each function have param `std::string_view additional=""`
// - add specific parameters to include important information
//
// And line numbers.
namespace issue
{
  inline void syntax_error (Issues& issues, const TSNode& node)
  {
    const auto start = ts_node_start_point(node);
    const auto start_byte = ts_node_start_byte(node);
    const auto end_byte = ts_node_end_byte(node);

    issues.add_error(std::format("Syntax error at {}:{}", start.row+1, start.column+1), start_byte, end_byte, ErrorCode::SyntaxError);
  }

  inline void compiler_error (Issues& issues, const AstNode& node, const std::string_view msg = "")
  {
    // TODO get the call site
    issues.add_error(std::format("The compiler fell over: {}", msg), node.source, ErrorCode::Compiler);
  }

  inline void unknown_param_type (Issues& issues, const AstNode& node)
  {
    issues.add_error("Unknown parameter type", node.source, ErrorCode::UnknownParamType);
  }

  inline void unknown_variable_type (Issues& issues, const AstNode& node)
  {
    issues.add_error("Unknown variable type", node.source, ErrorCode::UnknownVarType);
  }

  inline void unknown_return_type(Issues& issues, const std::string_view func, const std::string_view type)
  {
    issues.add_error(std::format("Unknown return type '{}' for {}", type, func), ErrorCode::UnknownReturnType);
  }

  inline void module_not_exist (Issues& issues, const AstNode& node, const std::string_view func)
  {
    issues.add_error(std::format("Module does not exist: {}", func), node.source, ErrorCode::ModuleNotExist);
  }

  inline void func_duplicate (Issues& issues, const AstNode& node, const std::string_view func)
  {
    issues.add_error(std::format("Function already defined: {}", func), node.source, ErrorCode::FunctionDuplicate);
  }

  inline void func_not_exist (Issues& issues, const AstNode& node, const std::string_view func)
  {
    issues.add_error(std::format("Function does not exist: {}", func), node.source, ErrorCode::FunctionNotExist);
  }

  inline void func_arg (Issues& issues, const AstNode& node, const std::string_view arg_name)
  {
    issues.add_error(std::format("Function call has invalid type for argument: {}", arg_name), node.source, ErrorCode::FunctionCallArgType);
  }

  inline void func_arg_void (Issues& issues, const AstNode& node, const std::string_view arg_name)
  {
    issues.add_error(std::format("Parameter '{}' in function call is void", arg_name), node.source, ErrorCode::FunctionCallArgVoid);
  }

  inline void func_arg_count (Issues& issues, const AstNode& node)
  {
    issues.add_error("Function call with incorrect number of arguments", node.source, ErrorCode::FunctionCallArgsCount);
  }

  inline void func_arg_ref_required (Issues& issues, const AstNode& node)
  {
    issues.add_error("Function argument requires explicit 'ref'", node.source, ErrorCode::FunctionCallRefRequired);
  }

  inline void func_arg_not_ref (Issues& issues, const AstNode& node)
  {
    issues.add_error("Function parameter is not a reference", node.source, ErrorCode::FunctionCallParamNotRef);
  }

  inline void func_arg_ref_type (Issues& issues, const AstNode& node)
  {
    issues.add_error("A ref parameter must be a variable", node.source, ErrorCode::FunctionCallParamRefTypeInvalid);
  }

  inline void return_type_from_func (Issues& issues, const AstNode& node)
  {
    issues.add_error("Return type of expression incompatible with function", node.source, ErrorCode::FunctionReturnExpression);
  }

  inline void func_return_val_expected(Issues& issues, const AstNode& node)
  {
    issues.add_error("Function does not return value", node.source, ErrorCode::FunctionReturnExprExpected);
  }

  inline void var_duplicate (Issues& issues, const AstNode& node)
  {
    issues.add_error("Variable already defined", node.source, ErrorCode::VariableDuplicate);
  }

  inline void var_init_void (Issues& issues, const AstNode& node)
  {
    issues.add_error("Cannot intialise variable from void", node.source, ErrorCode::VariableInitVoid);
  }

  inline void var_init_type_differ (Issues& issues, const AstNode& node)
  {
    issues.add_error("Variable initialised type incompatible from explicit type", node.source, ErrorCode::VariableInitType);
  }

  inline void var_assign_type_differ (Issues& issues, const AstNode& node)
  {
    issues.add_error("Variable assign type incompatible with expression", node.source, ErrorCode::VariableAssignType);
  }

  // TODO decide: don't think we need the 'var' param since displayed from node.source
  inline void var_unknown (Issues& issues, const AstNode& node, const std::string_view var)
  {
    issues.add_error(std::format("Variable not declared: {}", var), node.source, ErrorCode::VariableUnknown);
  }

  inline void unsupported (Issues& issues, const AstNode& node, const std::string_view feature)
  {
    issues.add_error(std::format("Unsupported feature: {}", feature), node.source, ErrorCode::Unsupported);
  }

  inline void binary_operands_invalid (Issues& issues, const AstNode& node)
  {
    issues.add_error("Binary expression operands have incompatible types", node.source, ErrorCode::BinaryOperandsInvalid);
  }

  inline void binary_operands_void (Issues& issues, const AstNode& node)
  {
    issues.add_error("Void operand(s) in binary expression", node.source, ErrorCode::BinaryOperandsVoid);
  }

  inline void condition_not_bool (Issues& issues, const AstNode& node)
  {
    issues.add_error("Condition expression is not boolean", node.source, ErrorCode::ConditionNotBool);
  }

  inline void return_from_root (Issues& issues, const AstNode& node)
  {
    issues.add_error("Cannot return from script root", node.source, ErrorCode::ReturnFromRoot);
  }
}
