#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
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


std::string_view from_source_file (const Source& src, const TSNode& node)
{
  const auto start = ts_node_start_byte(node);
  return src.src.substr(start, ts_node_end_byte(node) - start);
}


void create_issue (Issues& issues, const TSNode& node)
{
  const auto start = ts_node_start_point(node);
  const auto start_byte = ts_node_start_byte(node);
  const auto end_byte = ts_node_end_byte(node);

  issues.add_error(std::format("Syntax error at {}:{}", start.row+1, start.column+1), start_byte, end_byte);
}

void create_issue (Issues& issues, const std::string_view m, const uint32_t from, const uint32_t to)
{
  issues.add_error(m, from, to);
}

void create_issue (Issues& issues, const AstNode& node, const std::string_view m)
{
  issues.add_error(m, node.source);
}

std::optional<VarType> create_type (const Source& src, const TSNode& node, Issues& issues)
{
  if (const auto name = from_source_file(src, node); name == "int") {
    return VarType{BuiltInType::Int};
  }
  else if (name == "str") {
    return VarType{BuiltInType::String};
  }
  else {
    create_issue(issues, node);
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


std::unique_ptr<FunctionDef> parse_function(const Source& src, TSNode& ts_node, Issues& issues)
{
  auto ast_node = std::make_unique<FunctionDef>();
  set_source_region(ts_node, *ast_node);

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source_file(src, name_node);

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

        if (const auto type = create_type(src, param_type_node, issues) ; type)
        {
          auto& func_node = ast_node->params.emplace_back(*type, from_source_file(src, param_name_node));
          set_source_region(param_name_node, func_node);
        }
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
        const auto name_node = ts_node_child_by_field_name(func_call, "func_name", 9);
        const auto args_node = ts_node_child_by_field_name(func_call, "args", 4);
        const auto byte_start = ts_node_start_byte(func_call);
        const auto byte_end = ts_node_end_byte(func_call);

        if (ts_node_is_null(name_node))
          continue;

        std::vector<FunctionArg> args;
        if (!ts_node_is_null(args_node)) {
          const auto n_args = ts_node_named_child_count(args_node);
          for (uint32_t arg = 0 ; arg < n_args ; ++arg)
          {
            const auto expr_node = ts_node_named_child(args_node, arg);
            if (const std::string_view expr_type = ts_node_type(ts_node_named_child(expr_node, 0)); !expr_type.empty())
            {
              const std::string_view value = from_source_file(src, ts_node_named_child(expr_node, 0));

              if (expr_type == "integer") {
                uint64_t i{};
                std::from_chars(value.data(), value.data()+value.size(), i);
                args.emplace_back(i);
              }
              else if (expr_type == "literal_string")
                args.emplace_back(value);
            }
          }
        }

        const auto func_name =  from_source_file(src, name_node);
        auto func_call_node = std::make_unique<FunctionCall>(func_name, std::move(args));
        set_source_region(*func_call_node, byte_start, byte_end);

        ast_node->body.nodes.push_back(std::move(func_call_node));
      }
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
      create_issue(issues, node);
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
  auto filter = [name = call.name](const std::unique_ptr<AstNode>& n)
  {
    return n->is_node_type(NodeType::FunctionDef) &&
           name == dynamic_cast<FunctionDef&>(*n).name;
  };

  for (const auto& func_def_node : root.nodes | vw::filter(filter))
  {
    const auto& def = dynamic_cast<FunctionDef&>(*func_def_node);
    return call.args.size() == def.params.size();
  }
  return false;
}

void semantic_checks(const Source& src, const SourceFile& root, Issues& issues)
{
  auto by_node_type = [](const NodeType nt)
  {
    return [nt](const std::unique_ptr<AstNode>& n){ return n->is_node_type(nt); };
  };

  for (const auto& func_def_node : root.nodes | vw::filter(by_node_type(NodeType::FunctionDef)))
  {
    for (const auto& func_call_node : dynamic_cast<FunctionDef&>(*func_def_node).body.nodes | vw::filter(by_node_type(NodeType::FunctionCall)))
    {
      const auto& func_call = dynamic_cast<FunctionCall&>(*func_call_node);
      if (!does_function_call_exist(root, func_call)) {
        create_issue(issues, func_call, "Function does not exist");
      }
    }
  }
}

int main (int argc, char ** argv)
{
  TSParser * parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_cpy());

  const std::string_view source_code = R"(
    fn hello(a: int) -> str
    {

    }

    fn main(a: str) -> int
    {
      hello();
      hello(3);
      hello("world");
      hello(3, "world");
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

  if (!have_entry_point(*ast_root))
    issues.add_error("No entry function 'fn main (str:) -> int' found, or multiple definitions");

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  ast_root->dump(std::cout);

  semantic_checks(src, *ast_root, issues);

  issues.dump(std::cout, src.src);

  return issues.have_errors() ? 1 : 0;
}
