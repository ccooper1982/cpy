# TODO

## Logical Operators
- `&&`, `||`

## String Concatenation
```
a := "hello";
b := " cpy" ;
c = a + b;
```

- `operators.hpp` provide `operator+()` concatenation when `VarValue` is `std::string_view`
- Literal strings are stored in the AST as `std::string_view` but creating the new string returns a `std::string` (`CpyString`)


## Comments
```
# single line
/#
multiple
lines
#/
```


## for
```
for ([init_expr] ; <condition_expr:bool> ; [iteration_expr])
{
  <NodeBlock>
}
```

## Containers

### Sequence

List:
- All elements must have the same type
- Variable size (implemented with an `std::list` or `std::vector`)
- Denoted with `[` and `]`

```
list_ok := ["abc", "def", "ghi"];
list_ok.append("jkl");
```

### Associative
Map:
- All keys must the same type
- All values must be the same type
- Implemented with `std::map`
- Denoted with `{` and `}`

```
contacts := {
  "john": ["12345", "16 Smith Drive"],
  "david": ["6789", "16 Park Street"]
};
```

```
contacts := {
  "john" : {"dob":"10-10-1990", "post_code":"abc def", "city":"London"}
};
```

### for each
Something like:

```
for (<variable_decl_assign_expr> in <variable_ref:list|array>)
{
  <NodeBlock>
}
```

`variable_decl_assign_expr`:
- similar to `VariableDecl` and `VariableAssign` because it is declaring a variable and assigning in one expression
- type is inferred from `<variable_ref>` (the element type of the `list`/`map`)


list:
```
my_files = files::list("some/dir");
for (file in my_files) {

}
```

map:
```
contacts := {
  "john" : {"dob":"10-10-1990", "post_code":"abc def", "city":"London"}
};

for (name, details in contacts) {
  
}
```
