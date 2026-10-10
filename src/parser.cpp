#include "tree_sitter/api.h"
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

extern "C" const TSLanguage *tree_sitter_cpy();


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

static void set_source_region (const TSNode& ts_node, AstNode& ast_node)
{
  ast_node.source = SourceRegion{ts_node_start_byte(ts_node), ts_node_end_byte(ts_node)};
}

static void set_source_region (AstNode& ast_node, const uint32_t from, const uint32_t to)
{
  ast_node.source = SourceRegion{from, to};
}


std::unique_ptr<Expression> Parser::parse_expression(const TSNode& expr_node)
{
  std::unique_ptr<Expression> expr;

  const std::string_view expr_type = ts_node_type(ts_node_named_child(expr_node, 0));

  if (expr_type.empty())
    return nullptr;

  const std::string_view value = from_source(ts_node_named_child(expr_node, 0));

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

    const auto op = get_binary_operator(from_source(op_node));
    expr = std::make_unique<BinaryExpression>(parse_expression(lhs_node),
                                              parse_expression(rhs_node),
                                              op);
  }
  else if (expr_type == "function_call") {
    expr = parse_function_call(ts_node_named_child(expr_node, 0));
  }
  else if (expr_type == "identifier") {
    expr = std::make_unique<VariableRef>(from_source(expr_node));
  }

  if (expr) {
    set_source_region(expr_node, *expr);
  }
  else {
    throw std::runtime_error{std::format("Unknown expression: {}", from_source(expr_node))};
  }

  return expr;
}


std::unique_ptr<VariableDecl> Parser::parse_variable_decl(const TSNode& ts_node)
{
  const auto name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  const auto type_node = ts_node_child_by_field_name(ts_node, "type_name", 9);
  const auto init_node = ts_node_child_by_field_name(ts_node, "initialiser", 11);
  const auto have_type = !ts_node_is_null(type_node);
  const auto have_init = !ts_node_is_null(init_node);

  // name always required. If we don't have a type, we must have an initialiser
  if (ts_node_has_error(ts_node) || ts_node_is_null(name_node) || (!have_type && !have_init))
  {
    issue::syntax_error(*m_issues, ts_node);
    return nullptr;
  }

  std::unique_ptr<VariableDecl> var_decl;

  if (have_type)
  {
    var_decl = std::make_unique<VariableDecl>(VariableDecl::create_explicit(have_init ? parse_expression(init_node) : nullptr));
    var_decl->var_type = from_source(type_node);
  }
  else {
    var_decl = std::make_unique<VariableDecl>(VariableDecl::create_inferred(parse_expression(init_node)));
  }

  var_decl->var_name = from_source(name_node);

  set_source_region(ts_node, *var_decl);
  return var_decl;
}


std::unique_ptr<AstNode> Parser::parse_statement (const TSNode& ts_statement)
{
  const auto func_call = ts_node_child_by_field_name(ts_statement, "func_call", 9);
  const auto var_decl = ts_node_child_by_field_name(ts_statement, "variable_declaration", 20);

  std::unique_ptr<AstNode> node;

  if (!ts_node_is_null(func_call)) {
    node = parse_function_call(func_call);
  }
  else if (!ts_node_is_null(var_decl)) {
    node = parse_variable_decl(var_decl);
  }

  return node;
}


std::vector<std::unique_ptr<AstNode>> Parser::parse_statements (const TSNode& ts_statements)
{
  std::vector<std::unique_ptr<AstNode>> statements;

  const auto statement_count = ts_node_named_child_count(ts_statements);
  statements.reserve(std::min<uint32_t>(statement_count, 30));

  for (uint32_t s = 0; s < statement_count; ++s)
  {
    const auto statement = ts_node_named_child(ts_statements, s);
    auto node = parse_statement(statement);
    if (node) {
      statements.push_back(std::move(node));
    }
  }

  return statements;
}


std::vector<std::unique_ptr<Expression>> Parser::parse_function_call_args(const TSNode& args_node)
{
  std::vector<std::unique_ptr<Expression>> args;
  if (!ts_node_is_null(args_node))
  {
    const auto n_args = ts_node_named_child_count(args_node);

    for (uint32_t arg = 0 ; arg < n_args ; ++arg)
    {
      const auto expr_node = ts_node_named_child(args_node, arg);
      args.push_back(parse_expression(expr_node));
    }
  }
  return args;
}


std::unique_ptr<FunctionCall> Parser::parse_function_call(const TSNode& func_call)
{
  const auto name_node = ts_node_child_by_field_name(func_call, "name", 4);
  const auto args_node = ts_node_child_by_field_name(func_call, "args", 4);

  const auto func_name = from_source(name_node);

  auto args = parse_function_call_args(args_node);

  auto func_call_node = std::make_unique<FunctionCall>(func_name, std::move(args));
  set_source_region(*func_call_node, ts_node_start_byte(func_call), ts_node_end_byte(func_call));

  if (func_name.contains("::")) {
    func_call_node->module = func_name.substr(0, func_name.find("::"));
  }

  return func_call_node;
}


std::unique_ptr<FunctionDef> Parser::parse_function_def(const TSNode& ts_node)
{
  auto ast_node = std::make_unique<FunctionDef>();
  set_source_region(ts_node, *ast_node);

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source(name_node);

  // return type
  TSNode return_type = ts_node_child_by_field_name(ts_node, "return_type", 11);
  if (!ts_node_is_null(return_type))
  {
    auto type_node = ts_node_child_by_field_name(return_type, "type_name", 9);
    ast_node->return_type = from_source(type_node);
  }
  else {
    ast_node->return_type = "void";
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
        param.param_name = from_source(param_name_node);
      }
      if (!ts_node_is_null(param_type_node)) {
        param.type_name = from_source(param_type_node);
      }

      set_source_region(parameter, param);

      ast_node->params.push_back(std::move(param));
    }
  }

  // body
  auto body = ts_node_child_by_field_name(ts_node, "body", 4);
  if (!ts_node_is_null(body))
  {
    if (auto statements = parse_statements(body); !statements.empty()) {
      ast_node->body.nodes = std::move(statements);
    }
  }

  return ast_node;
}


std::string_view Parser::from_source (const TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  const auto length = ts_node_end_byte(node) - start;

  return std::string_view{*m_src}.substr(start, length);
}


void Parser::parse_script(const TSNode& ts_root)
{
  const auto n_children = ts_node_named_child_count(ts_root);

  m_ast = std::make_unique<SourceFile>();
  m_ast->nodes.reserve(std::min(n_children, 20U));

  for (uint32_t i = 0 ; i < n_children ; ++i)
  {
    auto node = ts_node_named_child(ts_root, i);

    if (ts_node_is_error(node))
    {
      issue::syntax_error(*m_issues, node);
      m_ast->nodes.push_back(std::make_unique<SyntaxError>());
    }
    else
    {
      const std::string_view type = ts_node_type(node) ;

      if (type == "function_def")
      {
        if (auto def = parse_function_def(node); def) {
          m_ast->nodes.push_back(std::move(def));
        }
      }
      else if (type == "statement")
      {
        if (auto statement = parse_statement(node); statement) {
          m_ast->nodes.push_back(std::move(statement));
        }
      }
      else {
        throw std::runtime_error{std::format("Uknown node type {}", type)};
      }
    }
  }
}


void Parser::parse()
{
  m_parser.reset(ts_parser_new());

  ts_parser_set_language(m_parser.get(), tree_sitter_cpy());

  m_tree.reset(ts_parser_parse_string(m_parser.get(), nullptr, m_src->data(), m_src->length()));

  TSNode root = ts_tree_root_node(m_tree.get());

  if (const std::string_view root_type = ts_node_type(root) ; root_type != "source_file") {
    throw std::runtime_error{"Root is not a source_file"};
  }

  parse_script(root);
}


Script Parser::parse(const fs::path& src_file)
{
  if (std::ifstream stream(src_file); !stream) {
      throw std::runtime_error{"Failed to open source file: " + src_file.string()};
  }
  else {
    m_src = std::make_unique<std::string> (std::istreambuf_iterator<char>{stream},
                                           std::istreambuf_iterator<char>{});
  }

  m_issues = std::make_unique<Issues>(src_file);

  parse();

  Script script;
  script.file = src_file;
  script.ast = std::move(m_ast);
  script.issues = std::move(m_issues);
  script.src = std::move(m_src);

  return script;
}

Script Parser::parse(const std::string_view src)
{
  m_src = std::make_unique<std::string> (src);
  m_issues = std::make_unique<Issues>();

  parse();

  Script script;
  script.ast = std::move(m_ast);
  script.issues = std::move(m_issues);
  script.src = std::move(m_src);

  return script;
}
