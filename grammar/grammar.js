export default grammar({
  name: "cpy",

  rules: {
    source_file: ($) => repeat($.function_def),

    function_def: ($) =>
      seq(
        "fn",
        field("name", $.identifier),
        "(",
        optional(field("parameters", $.parameters)),
        ")",
        optional(field("return_type", $.return_type)),
        field("body", $.function_body),
      ),

    parameters: ($) => seq($.parameter, repeat(seq(",", $.parameter))),
    parameter: ($) =>
      seq(field("name", $.identifier), ":", field("type", $.type)),
    return_type: ($) => seq("->", field("type", $.type)),
    function_body: ($) => seq("{", "}"),
    type: ($) => $.identifier,
    identifier: ($) => /[a-zA-Z_][a-zA-Z0-9_]*/,
  },
});
