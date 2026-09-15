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
    type: ($) => $.identifier,
    identifier: ($) => /[a-zA-Z_][a-zA-Z0-9_]*/,
    integer: ($) => /[0-9]+/,
    decimal: ($) => /[0-9]+\.[0-9]+/,
    literal_string: ($) => /"([^"\\]|\\.)*"/,

    function_body: ($) => seq(
      "{",
      repeat($.statement),
      "}",
    ),

    statement: ($) => choice(
      field("func_call", $.function_call),
      ";"
    ),

    function_call: ($) => seq(
      field("name", $.identifier),
      "(",
      optional(field("args", $.arguments)),
      ")"
    ),

    arguments: ($) => seq(
      $.expression,
      repeat(seq(",", $.expression)),
    ),

    expression: ($) => choice(
      $.decimal,
      $.integer,
      $.literal_string,
      $.function_call,
      $.identifier,
    )
  },
});
