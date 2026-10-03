#include "cpy/semantics.hpp"
#include <cpy/ast/ast_node.hpp>
#include <cpy/issues.hpp>
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

// TODO tidy. It's becoming a mess.

std::unique_ptr<FunctionCall> parse_function_call(Script& script, const TSNode& func_call, Issues& issues);


static BinaryOperator get_binary_operator(const std::string_view op)
{
  if (op == "+")  return BinaryOperator::Add;
  if (op == "-")  return BinaryOperator::Subtract;
  if (op == "*")  return BinaryOperator::Multiply;
  if (op == "/")  return BinaryOperator::Divide;
  if (op == "==") return BinaryOperator::Equal;
  if (op == "!=") return BinaryOperator::NotEqual;
  if (op == "<")  return BinaryOperator::Less;
  if (op == ">")  return BinaryOperator::Greater;
  if (op == "<=") return BinaryOperator::LessEqual;
  if (op == ">=") return BinaryOperator::GreaterEqual;

  std::unreachable();
}

std::string_view from_source (const Script& script, const TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  const auto length = ts_node_end_byte(node) - start;

  return std::string_view{script.src}.substr(start, length);
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
    issue::unknown_type(issues, node);
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

std::unique_ptr<Expression> parse_expression(Script& script, const TSNode& expr_node)
{
  std::unique_ptr<Expression> expr;

  const std::string_view expr_type = ts_node_type(ts_node_named_child(expr_node, 0));

  if (!expr_type.empty())
  {
    const std::string_view value = from_source(script, ts_node_named_child(expr_node, 0));

    if (expr_type == "integer")
    {
      int64_t i{};
      std::from_chars(value.data(), value.data()+value.size(), i);
      expr = std::make_unique<IntegerLiteral>(i);
    }
    else if (expr_type == "literal_string") {
      expr = std::make_unique<StringLiteral>(value);
    }
    else if (expr_type == "decimal")
    {
      double d{};
      std::from_chars(value.data(), value.data()+value.size(), d);
      expr = std::make_unique<DecimalLiteral>(d);
    }
    else if (expr_type == "boolean") {
      expr = std::make_unique<BooleanLiteral>(value == "true");
    }
    else if (expr_type == "binary_expression")
    {
      const auto bin_expr_node = ts_node_named_child(expr_node, 0);
      const auto lhs_node = ts_node_child_by_field_name(bin_expr_node, "lhs", 3);
      const auto op_node = ts_node_child_by_field_name(bin_expr_node, "op", 2);
      const auto rhs_node = ts_node_child_by_field_name(bin_expr_node, "rhs", 3);

      const auto op = get_binary_operator(from_source(script, op_node));
      expr = std::make_unique<BinaryExpression>(parse_expression(script, lhs_node),
                                                parse_expression(script, rhs_node),
                                                op);
    }
    else if (expr_type == "function_call")
    {
      expr = parse_function_call(script, ts_node_named_child(expr_node, 0), *script.issues);
    }
  }
  return expr;
}

std::vector<std::unique_ptr<Expression>> parse_function_call_args(Script& script, const TSNode& args_node)
{
  std::vector<std::unique_ptr<Expression>> args;
  if (!ts_node_is_null(args_node))
  {
    const auto n_args = ts_node_named_child_count(args_node);

    for (uint32_t arg = 0 ; arg < n_args ; ++arg)
    {
      const auto expr_node = ts_node_named_child(args_node, arg);
      args.push_back(parse_expression(script, expr_node));
    }
  }
  return args;
}

std::unique_ptr<FunctionCall> parse_function_call(Script& script, const TSNode& func_call, Issues& issues)
{
  const auto name_node = ts_node_child_by_field_name(func_call, "name", 4);
  const auto args_node = ts_node_child_by_field_name(func_call, "args", 4);

  const auto func_name = from_source(script, name_node);

  auto args = parse_function_call_args(script, args_node);

  auto func_call_node = std::make_unique<FunctionCall>(func_name, std::move(args));
  set_source_region(*func_call_node, ts_node_start_byte(func_call), ts_node_end_byte(func_call));

  if (func_name.contains("::")) {
    func_call_node->module = func_name.substr(0, func_name.find("::"));
  }

  return func_call_node;
}

std::unique_ptr<FunctionDef> parse_function_def(Script& script, const TSNode& ts_node, Issues& issues)
{
  auto ast_node = std::make_unique<FunctionDef>();
  set_source_region(ts_node, *ast_node);

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source(script, name_node);

  // return type
  TSNode return_type = ts_node_child_by_field_name(ts_node, "return_type", 11);
  if (!ts_node_is_null(return_type))
  {
    auto type_node = ts_node_child_by_field_name(return_type, "type", 4);
    ast_node->return_type = from_source(script, type_node);
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
        if (!ts_node_is_null(param_name_node)) {
          param.type_name = from_source(script, param_name_node);
        }
        if (!ts_node_is_null(param_type_node)) {
          param.type_name = from_source(script, param_type_node);
        }

        set_source_region(param_name_node, param);

        ast_node->params.push_back(std::move(param));
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

      if (!ts_node_is_null(func_call)) {
        ast_node->body.nodes.push_back(parse_function_call(script, func_call, issues));
      }
    }
  }

  return ast_node;
}


void parse_script(Script& script, TSNode& ts_root, Issues& issues)
{
  auto process_node = [&](const TSNode& node) -> std::unique_ptr<AstNode>
  {
    if (ts_node_is_error(node))
    {
      issue::syntax_error(issues, node);
      return std::make_unique<SyntaxError>();
    }

    const std::string_view type = ts_node_type(node) ;

    if (type == "function_def") {
      return parse_function_def(script, node, issues);
    }
    else if (type == "statement")
    {
      const auto statement_count = ts_node_named_child_count(node);

      for (uint32_t s = 0; s < statement_count; ++s)
      {
        const auto statement = ts_node_named_child(node, s);

        const std::string_view type = ts_node_type(statement);

        if (type == "function_call") {
          return parse_function_call(script, statement, issues);
        }
      }

      return nullptr; // probably a ';'
    }
    else {
      throw std::runtime_error{std::format("Uknown node type {}", type)};
    }
  };

  const auto n_children = ts_node_named_child_count(ts_root);

  script.ast = std::make_unique<SourceFile>();
  script.ast->nodes.reserve(n_children); // TODO set limits

  for (uint32_t i = 0 ; i < n_children ; ++i)
  {
    auto child = ts_node_named_child(ts_root, i);
    if (auto node = process_node(child); node) {
      script.ast->nodes.push_back(std::move(node));
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

  parse_script(script, root, *script.issues);
}

Script Parser::parse(const fs::path src_file)
{
  Script script;
  script.file = src_file;
  script.issues = std::make_unique<Issues>(src_file);

  std::ifstream stream(src_file);
  if (!stream) {
      throw std::runtime_error{"Failed to open source file: " + src_file.string()};
  }

  script.src = {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};

  parse(script);
  return script;
}

Script Parser::parse(const std::string_view src)
{
  Script script;
  script.src = std::string{src};
  script.issues = std::make_unique<Issues>();

  parse(script);
  return script;
}
