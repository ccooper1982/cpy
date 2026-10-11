export default grammar({
  name: "cpy",

  rules: {
    source_file: ($) => repeat(choice(
      $.function_def,
      $.statement,
    )),

    function_def: ($) =>
      seq(
        "fn",
        field("name", $.identifier),
        "(",
        optional(field("parameters", $.parameters)),
        ")",
        optional(field("return_type", $.return_type)),
        field("body", $.block),
      ),

    parameters: ($) => seq($.parameter, repeat(seq(",", $.parameter))),
    parameter: ($) => seq(field("name", $.identifier), ":", field("type", $.type_name)),
    return_type: ($) => seq("->", field("type_name", $.type_name)),
    type_name: ($) => $.identifier,
    identifier: ($) => /[a-zA-Z_][a-zA-Z0-9_]*/,
    integer: ($) => /[0-9]+/,
    decimal: ($) => /[0-9]+\.[0-9]+/,
    boolean: ($) => choice("true","false",),
    literal_string: ($) => /"([^"\\]|\\.)*"/,

    block: ($) => seq(
      "{",
      repeat($.statement),
      "}",
    ),

    statement: ($) =>
      choice(
        seq(
          choice(
            field("func_call", $.function_call),
            field("variable_declaration", $.variable_declaration),
          ),
          ";"
        ),
      field("condition_if", $.condition_if),
      ),

    function_call: ($) => seq(
      field("name", $.qualified_name),
      "(",
      optional(field("args", $.arguments)),
      ")",
    ),

    condition_if: ($) =>
      seq(
        "if",
        "(",
        field("condition", $.expression),
        ")",
        field("body", $.block),
      ),

    variable_declaration: ($) =>
      seq(
        field("name", $.identifier),
        choice(
          seq(
            ":",
            field("type_name", $.type_name),
            optional(seq(
              "=",
              field("initialiser", $.expression),
            )),
          ),
          seq(
            ":=",
            field("initialiser", $.expression),
          ),
        ),
      ),

    qualified_name: ($) => seq(
      optional(seq($.identifier, "::")),
      $.identifier,
    ),

    arguments: ($) => seq(
      $.expression,
      repeat(seq(",", $.expression)),
    ),

    expression: ($) => choice(
      $.decimal,
      $.integer,
      $.literal_string,
      $.boolean,
      $.function_call,
      $.identifier,
      $.binary_expression
    ),

    // TODO operator precedence with prec(1,...), prec(2,...)
    binary_expression: ($) =>
      prec.left(seq(
        field("lhs", $.expression),
        field("op", choice("+", "-", "*", "/", "!=", "==", ">=", "<=", ">", "<")),
        field("rhs", $.expression),
      ))
  },
});
