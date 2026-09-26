#include "cpy/ast/ast_node.hpp"
#include "tree_sitter/api.h"
#include <cpy/parser.hpp>
#include <cpy/modules.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>


std::string_view from_source (const Script& script, const TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  const auto length = ts_node_end_byte(node) - start;

  return std::string_view{script.src}.substr(start, length);
}

void create_issue_syntax_error (Issues& issues, const TSNode& node)
{
  const auto start = ts_node_start_point(node);
  const auto start_byte = ts_node_start_byte(node);
  const auto end_byte = ts_node_end_byte(node);

  issues.add_error(std::format("Syntax error at {}:{}", start.row+1, start.column+1), start_byte, end_byte);
}

void create_issue_unknown_type (Issues& issues, const TSNode& node)
{
  const auto start = ts_node_start_point(node);
  const auto start_byte = ts_node_start_byte(node);
  const auto end_byte = ts_node_end_byte(node);

  issues.add_error(std::format("Unknown type at {}:{}", start.row+1, start.column+1), start_byte, end_byte);
}

void create_issue (Issues& issues, const std::string_view m, const uint32_t from, const uint32_t to)
{
  issues.add_error(m, from, to);
}

void create_issue (Issues& issues, const AstNode& node, const std::string_view m)
{
  issues.add_error(m, node.source);
}

std::optional<VarType> create_type (const Script& src, const TSNode& node, Issues& issues)
{
  const auto name = from_source(src, node);
  if ( name == "int") {
    return BuiltInType::Int;
  }
  else if (name == "str") {
    return BuiltInType::String;
  }
  else if (name == "dec") {
    return BuiltInType::Decimal;
  }
  else if (name == "bool") {
    return BuiltInType::Bool;
  }
  else {
    create_issue_unknown_type(issues, node);
    return std::nullopt;
  }
}

void set_source_region (const TSNode& ts_node, AstNode& ast_node)
{
  ast_node.source = SourceRegion{ts_node_start_byte(ts_node), ts_node_end_byte(ts_node)};
}

void set_source_region (AstNode& ast_node, const uint32_t from, const uint32_t to)
{
  ast_node.source = SourceRegion{from, to};
}

std::vector<std::unique_ptr<Expression>> parse_function_args(const Script& src, const TSNode& args_node)
{
  std::vector<std::unique_ptr<Expression>> args;
  if (!ts_node_is_null(args_node)) {
    const auto n_args = ts_node_named_child_count(args_node);

    for (uint32_t arg = 0 ; arg < n_args ; ++arg)
    {
      const auto expr_node = ts_node_named_child(args_node, arg);
      const std::string_view expr_type = ts_node_type(ts_node_named_child(expr_node, 0));
      if (!expr_type.empty())
      {
        const std::string_view value = from_source(src, ts_node_named_child(expr_node, 0));

        if (expr_type == "integer")
        {
          int64_t i{};
          std::from_chars(value.data(), value.data()+value.size(), i);
          args.emplace_back(std::make_unique<IntegerLiteral>(i));
        }
        else if (expr_type == "literal_string")
        {
          args.emplace_back(std::make_unique<StringLiteral>(value));
        }
        else if (expr_type == "decimal")
        {
          double d{};
          std::from_chars(value.data(), value.data()+value.size(), d);
          args.emplace_back(std::make_unique<DecimalLiteral>(d));
        }
        else if (expr_type == "boolean")
        {
          args.emplace_back(std::make_unique<BooleanLiteral>(value == "true"));
        }
      }
    }
  }
  return args;
}

std::unique_ptr<FunctionCall> parse_function_call(const Script& src, const TSNode& func_call, Issues& issues)
{
  const auto name_node = ts_node_child_by_field_name(func_call, "name", 4);
  const auto args_node = ts_node_child_by_field_name(func_call, "args", 4);

  const auto func_name = from_source(src, name_node);

  auto args = parse_function_args(src, args_node);

  auto func_call_node = std::make_unique<FunctionCall>(func_name, std::move(args));
  set_source_region(*func_call_node, ts_node_start_byte(func_call), ts_node_end_byte(func_call));

  if (func_name.contains("::"))
  {
    // TODO move to semantic checks
    func_call_node->module = func_name.substr(0, func_name.find_first_of(':'));

    if (!Modules::exist(func_call_node->module)){
      create_issue(issues, *func_call_node, std::format("Module does not exist: {}", func_call_node->module));
    }
  }

  return func_call_node;
}

std::unique_ptr<FunctionDef> parse_function(const Script& src, const TSNode& ts_node, Issues& issues)
{
  auto ast_node = std::make_unique<FunctionDef>();
  set_source_region(ts_node, *ast_node);

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source(src, name_node);

  // return type
  TSNode return_type = ts_node_child_by_field_name(ts_node, "return_type", 11);
  if (!ts_node_is_null(return_type)) {
    auto type_node = ts_node_child_by_field_name(return_type, "type", 4);
    if (const auto type = create_type(src, type_node, issues); type)
      ast_node->return_type = *type;
  }

  // params
  TSNode parameters = ts_node_child_by_field_name(ts_node, "parameters", 10);

  if (!ts_node_is_null(parameters))
  {
    const uint32_t param_count = ts_node_named_child_count(parameters);

    ast_node->params.reserve(param_count);

    for (uint32_t p = 0; p < param_count; ++p)
    {
        TSNode parameter = ts_node_named_child(parameters, p);
        TSNode param_name_node = ts_node_child_by_field_name(parameter, "name", 4);
        TSNode param_type_node = ts_node_child_by_field_name(parameter, "type", 4);

        FunctionParam param;
        if (const auto type = create_type(src, param_type_node, issues) ; type) {
          param = ast_node->params.emplace_back(*type, from_source(src, param_name_node));
        }
        else {
          param = ast_node->params.emplace_back(from_source(src, param_name_node));
        }

        set_source_region(param_name_node, param);
    }
  }

  // body
  auto body = ts_node_child_by_field_name(ts_node, "body", 4);
  if (!ts_node_is_null(body))
  {
    const auto statement_count = ts_node_named_child_count(body);
    ast_node->body.nodes.reserve(std::min<uint32_t>(statement_count, 30));

    for (uint32_t s = 0; s < statement_count; ++s)
    {
      const auto statement = ts_node_named_child(body, s);
      const auto func_call = ts_node_child_by_field_name(statement, "func_call", 9);

      if (!ts_node_is_null(func_call))
      {
        ast_node->body.nodes.push_back(parse_function_call(src, func_call, issues));
      }
    }
  }

  return ast_node;
}


void parse_script(Script& src, TSNode& ts_root, Issues& issues)
{
  auto process_node = [&](const TSNode& node) -> std::unique_ptr<AstNode>
  {
    if (ts_node_is_error(node))
    {
      create_issue_syntax_error(issues, node);
      return std::make_unique<SyntaxError>();
    }

    const std::string_view type = ts_node_type(node) ;

    if (type == "function_def") {
      return parse_function(src, node, issues);
    }
    else if (type == "statement") {
      const auto statement_count = ts_node_named_child_count(node);

      for (uint32_t s = 0; s < statement_count; ++s)
      {
        const auto statement = ts_node_named_child(node, s);

        const std::string_view type = ts_node_type(statement);

        if (type == "function_call") {
          return parse_function_call(src, statement, issues);
        }
      }

      return nullptr; // probably a ';'
    }
    else {
      throw std::runtime_error{std::format("Uknown node type {}", type)};
    }
  };

  const auto n_children = ts_node_named_child_count(ts_root);

  src.ast = std::make_unique<SourceFile>();
  src.ast->nodes.reserve(n_children); // TODO set limits

  for (uint32_t i = 0 ; i < n_children ; ++i)
  {
    auto child = ts_node_named_child(ts_root, i);
    if (auto node = process_node(child); node) {
      src.ast->nodes.push_back(std::move(node));
    }
  }
}


bool does_function_exist(const SourceFile& src, const std::string_view name, const VarType return_type, const std::vector<FunctionParam>& params, const bool check_param_names = false)
{
  return rg::find_if(src.nodes, [&](const auto& node) {
          if (!node->is_node_type(NodeType::FunctionDef))
            return false;

          const auto& func = dynamic_cast<const FunctionDef&>(*node);

          if (func.return_type != return_type || func.name != name)
            return false;

          return check_param_names ? rg::equal(func.params, params, FunctionParamCmp{})
                                   : rg::equal(func.params, params, FunctionParamCmpIgnoreName{});
         }) != src.nodes.cend();
}

std::uint16_t count_function_definitions (const SourceFile& src, const std::string_view name)
{
  return rg::count_if(src.nodes, [&](const auto& node) {
            if (!node->is_node_type(NodeType::FunctionDef))
              return false;
            return dynamic_cast<const FunctionDef&>(*node).name == name;
         });
}

bool have_entry_point(const SourceFile& src)
{
  return does_function_exist(src, "main", BuiltInType::Int,  {FunctionParam{BuiltInType::String}}) &&
         count_function_definitions(src, "main") == 1U;
}

// semantics

bool does_function_call_exist(const SourceFile& root, const FunctionCall& call)
{
  auto only_func_defs = [](const std::unique_ptr<AstNode>& n)
  {
    return n->is_node_type(NodeType::FunctionDef);
  };

  for (const auto& func_def_node : root.nodes | vw::filter(only_func_defs))
  {
    const auto& def = dynamic_cast<FunctionDef&>(*func_def_node);
    if (func_call_valid(def, call))
      return true;
  }
  return false;
}

void semantic_checks(const Script& src, const SourceFile& root, Issues& issues)
{
  auto by_node_type = [](const NodeType nt)
  {
    return [nt](const std::unique_ptr<AstNode>& n){ return n->is_node_type(nt); };
  };

  // function calls
  for (const auto& func_call_node : root.nodes | vw::filter(by_node_type(NodeType::FunctionCall)))
  {
    const auto& func_call = dynamic_cast<FunctionCall&>(*func_call_node);
    if (!does_function_call_exist(root, func_call)) {
      create_issue(issues, func_call, "Function does not exist");
    }
  }
}

Parser::~Parser()
{
  if (m_tree)
    ts_tree_delete(m_tree);
  if (m_parser)
    ts_parser_delete(m_parser);
}

void Parser::parse(Script& script)
{
  m_parser = ts_parser_new();

  ts_parser_set_language(m_parser, tree_sitter_cpy());

  m_tree = ts_parser_parse_string(m_parser, nullptr, script.src.data(), script.src.length());

  TSNode root = ts_tree_root_node(m_tree);

  if (const std::string_view root_type = ts_node_type(root) ; root_type != "source_file") {
    throw std::runtime_error{"Root is not a source_file"};
  }

  parse_script(script, root, script.issues);

  semantic_checks(script, *script.ast, script.issues);
}

std::expected<Script, CpyError> Parser::parse(const fs::path src_file)
{
  Script script {.file = src_file};

  std::ifstream stream(src_file);
  if (!stream) {
    return make_error<Script>("Failed to open file");
  }

  script.src = {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};

  parse(script);

  return script;
}

Script Parser::parse(const std::string_view src)
{
  Script script { .src = std::string(src.data(), src.size()) };

  parse(script);

  return script;
}
