#include <cpy/ast/ast.hpp>
#include <cpy/ast/expressions.hpp>
#include <cpy/common.hpp>
#include <cpy/issues.hpp>
#include <cpy/modules.hpp>
#include <cpy/parser.hpp>

#include "tree_sitter/api.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>


extern "C" const TSLanguage *tree_sitter_cpy();


template<typename NodeT, typename ...Args>
static NodePtr<NodeT> make_node (Args&&... args)
{
  return std::make_unique<NodeT>(std::forward<Args>(args)...);
}

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

/// ts_node_child_by_field_name() requires the field name and its length, excluding
/// the terminating null. This is a wrapper around that.
template <std::size_t N>
TSNode child_by_field_name(TSNode node, const char (&name)[N])
{
  return ts_node_child_by_field_name(node, name, N - 1);
}


std::string_view Parser::from_source (const TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  const auto length = ts_node_end_byte(node) - start;

  return std::string_view{*m_src}.substr(start, length);
}

NodePtr<Expression> Parser::parse_expression(const TSNode& expr_node)
{
  NodePtr<Expression> expr;

  const std::string_view expr_type = ts_node_type(ts_node_named_child(expr_node, 0));

  if (expr_type.empty())
    return nullptr;

  const std::string_view value = from_source(ts_node_named_child(expr_node, 0));

  if (expr_type == "integer")
  {
    CpyInt i{};
    std::from_chars(value.data(), value.data()+value.size(), i);
    expr = make_node<IntegerLiteral>(i);
  }
  else if (expr_type == "string")
  {
    // from_source() returns the string including the start and end '"' chars,
    // we don't want those:
    //  "foo" -> foo
    //  "" -> empty string
    expr = make_node<StringLiteral>(value.substr(1, value.size()-2));
  }
  else if (expr_type == "decimal")
  {
    CpyDec d{};
    std::from_chars(value.data(), value.data()+value.size(), d);
    expr = make_node<DecimalLiteral>(d);
  }
  else if (expr_type == "boolean") {
    expr = make_node<BooleanLiteral>(value == "true");
  }
  else if (expr_type == "binary_expression")
  {
    const auto bin_expr_node = ts_node_named_child(expr_node, 0);
    const auto lhs_node = child_by_field_name(bin_expr_node, "lhs");
    const auto op_node = child_by_field_name(bin_expr_node, "op");
    const auto rhs_node = child_by_field_name(bin_expr_node, "rhs");

    const auto op = get_binary_operator(from_source(op_node));
    expr = make_node<BinaryExpression>(parse_expression(lhs_node), parse_expression(rhs_node), op);
  }
  else if (expr_type == "function_call") {
    expr = parse_function_call(ts_node_named_child(expr_node, 0));
  }
  else if (expr_type == "identifier") {
    expr = make_node<VariableUse>(from_source(expr_node));
  }

  if (expr) {
    set_source_region(expr_node, *expr);
  }
  else {
    throw std::runtime_error{std::format("Unknown expression: {}", from_source(expr_node))};
  }

  return expr;
}


NodePtr<VariableDecl> Parser::parse_variable_decl(const TSNode& ts_node)
{
  const auto name_node = child_by_field_name(ts_node, "name");
  const auto type_node = child_by_field_name(ts_node, "type_name");
  const auto init_node = child_by_field_name(ts_node, "initialiser");
  const auto have_type = !ts_node_is_null(type_node);
  const auto have_init = !ts_node_is_null(init_node);

  // name always required. If we don't have a type, we must have an initialiser
  if (ts_node_has_error(ts_node) || ts_node_is_null(name_node) || (!have_type && !have_init))
  {
    issue::syntax_error(*m_issues, ts_node);
    return nullptr;
  }

  NodePtr<VariableDecl> var_decl;

  if (have_type)
  {
    var_decl = make_node<VariableDecl>(VariableDecl::create_explicit(have_init ? parse_expression(init_node) : nullptr));
    var_decl->var_type = from_source(type_node);
  }
  else {
    var_decl = make_node<VariableDecl>(VariableDecl::create_inferred(parse_expression(init_node)));
  }

  var_decl->var_name = from_source(name_node);

  set_source_region(ts_node, *var_decl);
  return var_decl;
}


NodePtr<VariableAssign> Parser::parse_variable_assign(const TSNode& ts_node)
{
  if (ts_node_has_error(ts_node))
  {
    issue::syntax_error(*m_issues, ts_node);
    return nullptr;
  }

  const auto name = child_by_field_name(ts_node, "name");
  const auto expr = child_by_field_name(ts_node, "value");

  auto assign_node = make_node<VariableAssign>() ;
  assign_node->name = from_source(name);
  assign_node->expr = parse_expression(expr);
  set_source_region(ts_node, *assign_node);
  return assign_node;
}


NodePtr<Return> Parser::parse_return(const TSNode& ts_node)
{
  const auto ret_expr = child_by_field_name(ts_node, "expression");

  auto node = make_node<Return>();

  if (!ts_node_is_null(ret_expr)) {
    node->return_expr = parse_expression(ret_expr);
  }

  set_source_region(ts_node, *node);

  return node;
}


NodeBlock::Nodes Parser::parse_statements (const TSNode& ts_statements)
{
  NodeBlock::Nodes statements;

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


NodePtr<AstNode> Parser::parse_statement (const TSNode& ts_statement)
{
  const auto func_call = child_by_field_name(ts_statement, "func_call");
  const auto var_decl = child_by_field_name(ts_statement, "var_decl");
  const auto cond_if = child_by_field_name(ts_statement, "condition_if");
  const auto var_assign = child_by_field_name(ts_statement, "var_assign");
  const auto return_ = child_by_field_name(ts_statement, "return");

  NodePtr<AstNode> node;

  if (ts_node_has_error(ts_statement)) {
    issue::syntax_error(*m_issues, ts_statement);
  }
  else if (!ts_node_is_null(func_call)) {
    node = parse_function_call(func_call);
  }
  else if (!ts_node_is_null(var_decl)) {
    node = parse_variable_decl(var_decl);
  }
  else if(!ts_node_is_null(cond_if)) {
    node = parse_condition_if (cond_if);
  }
  else if(!ts_node_is_null(var_assign)) {
    node = parse_variable_assign (var_assign);
  }
  else if(!ts_node_is_null(return_)) {
    node = parse_return (return_);
  }

  return node;
}


std::vector<FunctionArgument> Parser::parse_function_call_args(const TSNode& args_node)
{
  std::vector<FunctionArgument> args;
  if (!ts_node_is_null(args_node))
  {
    const auto n_args = ts_node_named_child_count(args_node);

    for (uint32_t i = 0 ; i < n_args ; ++i)
    {
      const auto arg_node = ts_node_named_child(args_node, i);
      const auto ref_node = child_by_field_name(arg_node, "ref");
      const auto expr_node = child_by_field_name(arg_node, "expression");

      FunctionArgument arg;
      arg.expr = parse_expression(expr_node);
      arg.is_ref = !ts_node_is_null(ref_node);

      args.push_back(std::move(arg));
    }
  }
  return args;
}


NodePtr<FunctionCall> Parser::parse_function_call(const TSNode& func_call)
{
  const auto name_node = child_by_field_name(func_call, "name");
  const auto args_node = child_by_field_name(func_call, "args");

  const auto func_name = from_source(name_node);

  auto args = parse_function_call_args(args_node);

  auto func_call_node = make_node<FunctionCall>(func_name, std::move(args));
  set_source_region(*func_call_node, ts_node_start_byte(func_call), ts_node_end_byte(func_call));

  if (func_name.contains("::")) {
    func_call_node->module = func_name.substr(0, func_name.find("::"));
  }

  return func_call_node;
}


NodePtr<FunctionDef> Parser::parse_function_def(const TSNode& ts_node)
{
  auto ast_node = make_node<FunctionDef>();
  set_source_region(ts_node, *ast_node);

  // name
  TSNode name_node = child_by_field_name(ts_node, "name");
  ast_node->name = from_source(name_node);

  // return type
  TSNode return_type = child_by_field_name(ts_node, "return_type");
  if (!ts_node_is_null(return_type))
  {
    auto type_node = child_by_field_name(return_type, "type_name");
    ast_node->return_type = from_source(type_node);
  }
  else {
    ast_node->return_type = "void";
  }

  // params
  TSNode parameters = child_by_field_name(ts_node, "parameters");

  if (!ts_node_is_null(parameters))
  {
    FunctionDef::FunctionParams params;

    const uint32_t param_count = ts_node_named_child_count(parameters);

    // ast_node->params.reserve(param_count);

    for (uint32_t p = 0; p < param_count; ++p)
    {
      TSNode parameter = ts_node_named_child(parameters, p);
      TSNode param_name_node = child_by_field_name(parameter, "name");
      TSNode param_type_node = child_by_field_name(parameter, "type");
      TSNode ref_node = child_by_field_name(parameter, "ref");

      FunctionParam param;
      set_source_region(parameter, param);

      if (!ts_node_is_null(param_name_node)) {
        param.param_name = from_source(param_name_node);
      }
      if (!ts_node_is_null(param_type_node)) {
        param.type_name = from_source(param_type_node);
      }

      param.is_ref = !ts_node_is_null(ref_node);
      params.push_back(std::move(param));
    }

    ast_node->set_params(std::move(params));
  }

  // body
  auto body = child_by_field_name(ts_node, "body");
  if (!ts_node_is_null(body))
  {
    set_source_region(body, ast_node->body);
    if (auto statements = parse_statements(body); !statements.empty()) {
      ast_node->body.set_nodes(std::move(statements));
    }
  }

  return ast_node;
}


NodePtr<ConditionIf> Parser::parse_condition_if(const TSNode& ts_node)
{
  if (ts_node_has_error(ts_node))
  {
    issue::syntax_error(*m_issues, ts_node);
    return nullptr;
  }

  auto parse_condition_body = [this](const TSNode& ts_node) -> ConditionIf::ConditionBody
  {
    const TSNode cond = child_by_field_name(ts_node, "condition");
    const TSNode body = child_by_field_name(ts_node, "body");

    ConditionIf::ConditionBody result;

    if (!ts_node_is_null(cond)) {
      result.condition = parse_expression(cond);
    }

    result.body.set_nodes(parse_statements(body));
    return result;
  };

  auto node = make_node<ConditionIf>();
  set_source_region(ts_node, *node);

  node->if_ = parse_condition_body(ts_node);

  for (uint32_t i = 0; i < ts_node_named_child_count(ts_node); ++i)
  {
    const TSNode child = ts_node_named_child(ts_node, i);
    const std::string_view type = ts_node_type(child);

    if (type == "else_if_clause")
    {
      node->else_if.push_back(parse_condition_body(child));
      set_source_region(child, node->else_if.back().body);
    }
    else if (type == "else_clause")
    {
      node->else_body = parse_condition_body(child).body;
      set_source_region(child, *node->else_body);
    }
  }

  return node;
}


void Parser::parse_script(const TSNode& ts_root)
{
  const auto n_children = ts_node_named_child_count(ts_root);

  m_ast = make_node<SourceFile>();

  set_source_region(ts_root, m_ast->block);

  for (uint32_t i = 0 ; i < n_children ; ++i)
  {
    const auto node = ts_node_named_child(ts_root, i);

    if (ts_node_is_error(node))
    {
      issue::syntax_error(*m_issues, node);
      m_ast->block.add_node(make_node<SyntaxError>());
    }
    else
    {
      const std::string_view type = ts_node_type(node) ;

      if (type == "function_def") {
        m_ast->block.add_node(parse_function_def(node));
      }
      else if (type == "statement") {
        m_ast->block.add_node(parse_statement(node));
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
