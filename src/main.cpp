#include <cpy/ast/ast_node.hpp>
#include <cpy/parser.hpp>


int main ([[maybe_unused]] int argc, [[maybe_unused]] char ** argv)
{
  const std::string_view src = R"(

    hello(4);
    hello("hello");
    files::exist("abc");

    fn hello(a: int) {}
    fn hello(a: arse) {}

  )";

  Parser parser;
  const auto script = parser.parse(src);

  script.issues.dump(std::cout, script.src);

  // TSParser * parser = ts_parser_new();

  // ts_parser_set_language(parser, tree_sitter_cpy());

  // const std::string_view source_code = R"(

  //   hello(4);
  //   hello("hello");
  //   files::exist("abc");

  //   fn hello(a: int) {}
  //   fn hello(a: arse) {}

  // )";

  // TSTree * tree = ts_parser_parse_string(parser, nullptr, source_code.data(), source_code.length());

  // TSNode root = ts_tree_root_node(tree);

  // if (const std::string_view root_type = ts_node_type(root) ; root_type != "source_file") {
  //   throw std::runtime_error{"Root is not a source_file"};
  // }

  // Issues issues{""}; // TODO file path

  // Script src { .src = source_code };
  // parse_source_file(src, root, issues);

  // // if (!have_entry_point(*ast))
  // //   issues.add_error("No entry function 'fn main (str:) -> int' found, or multiple definitions");

  // ts_tree_delete(tree);
  // ts_parser_delete(parser);

  // src.ast->dump(std::cout);

  // semantic_checks(src, *src.ast, issues);

  // issues.dump(std::cout, src.src);

  // return issues.have_errors() ? 1 : 0;
  return 0;
}
