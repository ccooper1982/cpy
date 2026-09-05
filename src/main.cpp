#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <tree_sitter/api.h>


namespace fs = std::filesystem;
namespace rg = std::ranges;
namespace vw = std::views;


extern "C" const TSLanguage *tree_sitter_cpy();

struct Issue
{
  Issue(const std::string_view m) : msg(m)
  {
  }

  std::string msg;
  // line
};

class Issues
{
  static constexpr std::string_view ColorRed   = "\033[31m";
  static constexpr std::string_view ColorReset = "\033[0m";

public:
  Issues (const fs::path src) : src_path(src) {}

  void add_error(const std::string_view msg)
  {
    m_errors.emplace_back(msg);
  }

  bool have_errors() const { return !m_errors.empty(); }

  void dump() const
  {
    if (m_errors.empty())
      return;

    std::cout << ColorRed << src_path << " : ERRORS " << ColorReset << '\n';

    for (const auto& err : m_errors)
      std::cout << "-> " << err.msg << '\n';
  }

private:
  fs::path src_path;
  std::vector<Issue> m_errors;
};

enum class NodeType
{
  None,
  SourceFile,
  Function,
  FunctionParam
};


struct AstNode
{
  virtual NodeType node_type() const = 0;

  virtual ~AstNode() = default;
  virtual void dump ([[maybe_unused]] const uint8_t tab = 0) const = 0;
};

struct FunctionParam : public AstNode
{
  std::string type;
  std::string name;

  FunctionParam()
  {

  }

  NodeType node_type() const override
  {
    return NodeType::FunctionParam;
  }


  void dump (const uint8_t tab = 0) const override
  {
    std::cout << std::string(tab*2, ' ') << name << ":" << type << '\n';
  }
};

struct FunctionParams //: public AstNode
{
  std::vector<FunctionParam> params;

  void dump ([[maybe_unused]] const uint8_t tab = 0) const// override
  {
    for(const auto& p : params)
      p.dump(tab);
  }
};

struct Function : public AstNode
{
  std::string name;
  FunctionParams params;
  std::string return_type;

  // FunctionBody body;
  // std::vector<std::unique_ptr<AstNode>> nodes;

  NodeType node_type() const override
  {
    return NodeType::Function;
  }

  void dump ([[maybe_unused]] const uint8_t tab = 0) const override
  {
    if (return_type.empty())
      std::cout << name << ":" << '\n';
    else
      std::cout << name << " -> " << return_type << ':' << '\n';

    params.dump(1);
  }
};

struct SourceFile : public AstNode
{
  std::vector<std::unique_ptr<AstNode>> nodes;
  fs::path src_path;

  NodeType node_type() const override
  {
    return NodeType::SourceFile;
  }

  void dump ([[maybe_unused]] const uint8_t tab = 0) const override
  {
    std::cout << (src_path.empty() ? "Compiled source" : src_path.string()) << '\n';

    for (const auto& n : nodes)
      n->dump();
  }
};


struct Source
{
  std::string_view src;
  // std::string file_name;
};


std::string_view from_source_file (const Source& src, TSNode& node)
{
  return src.src.substr(  ts_node_start_byte(node),
                          ts_node_end_byte(node) - ts_node_start_byte(node));
}


std::unique_ptr<Function> parse_function(const Source& src, TSNode& ts_node)
{
  auto ast_node = std::make_unique<Function>();

  // name
  TSNode name_node = ts_node_child_by_field_name(ts_node, "name", 4);
  ast_node->name = from_source_file(src, name_node);

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

    FunctionParams params;
    params.params.reserve(param_count);

    for (uint32_t p = 0; p < param_count; ++p)
    {
        TSNode parameter = ts_node_named_child(parameters, p);

        TSNode param_name_node = ts_node_child_by_field_name(parameter,"name",4);
        TSNode param_type_node = ts_node_child_by_field_name(parameter,"type",4);

        FunctionParam param;
        param.type = from_source_file(src, param_type_node);
        param.name = from_source_file(src, param_name_node);

        params.params.emplace_back(std::move(param));
    }

    ast_node->params = std::move(params);
  }

  return ast_node;
}


std::tuple<std::unique_ptr<SourceFile>, Issues> parse_source_file(const Source& src, TSNode& ts_root)
{
  auto process_node = [&src](TSNode& node, const std::string_view expect_type = "") -> std::unique_ptr<AstNode>
  {
    const std::string_view type = ts_node_type(node) ;

    if (!expect_type.empty() && type != expect_type)
      throw std::runtime_error{std::format("Unexpected node type {} != {}", type, expect_type)};

    if (type == "function_def") {
      return parse_function(src, node);
    }
    // else if (type == "source_file") {
    //   return parse_source_file(src, node);
    // }
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

  Issues issues{sf_node->src_path};

  return {std::move(sf_node), issues};
}


bool does_function_exist(const SourceFile& src, const std::string_view name)
{
  auto funcs = [](const std::unique_ptr<AstNode>& node){ return node->node_type() == NodeType::Function; };

  for (const auto& f : src.nodes | vw::filter(funcs))
    if (dynamic_cast<Function*>(f.get())->name == name)
      return true;
  return false;
}


int main (int argc, char ** argv)
{
  TSParser * parser = ts_parser_new();

  ts_parser_set_language(parser, tree_sitter_cpy());

  const std::string_view source_code = "fn foo(a: int) -> int {}";

  Source src { .src = source_code };

  TSTree * tree = ts_parser_parse_string(parser, nullptr, source_code.data(), source_code.length());

  TSNode root = ts_tree_root_node(tree);

  if (const std::string_view root_type = ts_node_type(root) ; root_type != "source_file") {
    throw std::runtime_error{"Root is not a source_file"};
  }

  auto [ast_root, issues] = parse_source_file(src, root);

  if (!does_function_exist(*ast_root, "main"))
    issues.add_error("No entry function 'main' found");

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  ast_root->dump();

  issues.dump();

  return issues.have_errors() ? 1 : 0;
}
