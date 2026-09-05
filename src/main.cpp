#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <cpy/ast/ast_node.hpp>
#include <cpy/issues.hpp>
#include <tree_sitter/api.h>


extern "C" const TSLanguage *tree_sitter_cpy();



struct Source
{
  std::string_view src;
  // fs::path path;
};


std::string_view from_source_file (const Source& src, TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  return src.src.substr(start, ts_node_end_byte(node) - start);
}


void create_issue (const TSNode& node, Issues& issues)
{
  const auto start = ts_node_start_point(node);
  const auto start_byte = ts_node_start_byte(node);
  const auto end_byte = ts_node_end_byte(node);

  issues.add_error(std::format("Syntax error at {}:{}", start.row+1, start.column+1), start_byte, end_byte);
}


std::unique_ptr<Function> parse_function(const Source& src, TSNode& ts_node, Issues& issues)
{
  auto ast_node = std::make_unique<Function>();

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source_file(src, name_node);

  // return type
  TSNode return_type = ts_node_child_by_field_name(ts_node, "return_type", 11);
  if (!ts_node_is_null(return_type)) {
    auto type_node = ts_node_child_by_field_name(return_type, "type", 4);
    ast_node->return_type = from_source_file(src, type_node);
  }

  // params
  TSNode parameters = ts_node_child_by_field_name(ts_node, "parameters", 10);

  if (!ts_node_is_null(parameters))
  {
    uint32_t param_count = ts_node_named_child_count(parameters);

    ast_node->params.reserve(param_count);

    for (uint32_t p = 0; p < param_count; ++p)
    {
        TSNode parameter = ts_node_named_child(parameters, p);
        TSNode param_name_node = ts_node_child_by_field_name(parameter, "name", 4);
        TSNode param_type_node = ts_node_child_by_field_name(parameter, "type", 4);

        ast_node->params.emplace_back(from_source_file(src, param_type_node), from_source_file(src, param_name_node));
    }
  }

  return ast_node;
}


std::unique_ptr<SourceFile> parse_source_file(const Source& src, TSNode& ts_root, Issues& issues)
{
  auto process_node = [&](TSNode& node) -> std::unique_ptr<AstNode>
  {
    if (ts_node_is_error(node))
    {
      create_issue(node, issues);
      return std::make_unique<Error>();
    }

    const std::string_view type = ts_node_type(node) ;

    if (type == "function_def") {
      return parse_function(src, node, issues);
    }
    else {
      throw std::runtime_error{std::format("Uknown node type {}", type)};
    }
  };

  auto sf_node = std::make_unique<SourceFile>();

  const auto n_children = ts_node_child_count(ts_root);

  for (uint32_t i = 0 ; i < n_children ; ++i)
  {
    auto child = ts_node_child(ts_root, i);
    sf_node->nodes.push_back(process_node(child));
  }

  return sf_node;
}


bool does_function_exist(const SourceFile& src, const std::string_view name)
{
  return rg::find_if(src.nodes, [&](const auto& node)
         {
           return node->is_node_type(NodeType::Function) &&
                  dynamic_cast<const Function&>(*node).name == name;
         }) != src.nodes.cend();
}


int main (int argc, char ** argv)
{
  TSParser * parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_cpy());

  const std::string_view source_code = R"(
    fn 12main(a: int) -> int
    {

    }

    fn hello(a: int) -> int
    {

    }
  )";

  Source src { .src = source_code };

  TSTree * tree = ts_parser_parse_string(parser, nullptr, source_code.data(), source_code.length());

  TSNode root = ts_tree_root_node(tree);

  if (const std::string_view root_type = ts_node_type(root) ; root_type != "source_file") {
    throw std::runtime_error{"Root is not a source_file"};
  }

  Issues issues{""}; // TODO file path
  auto ast_root = parse_source_file(src, root, issues);

  if (!does_function_exist(*ast_root, "main"))
    issues.add_error("No entry function 'main' found");

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  ast_root->dump(std::cout);

  issues.dump(std::cout, src.src);

  return issues.have_errors() ? 1 : 0;
}
