# TODO

## String Concatenation
```
a := "hello";
b := " cpy" ;
c = a + b;
```

- `operators.hpp` provide `operator+()` concatenation when `VarValue` is `std::string_view`
- Literal strings are stored in the AST as `std::string_view` but creating the new string returns a `std::string` (`CpyString`)

## Loops
- while
```
while (<expr:bool>)
{
  <NodeBlock>
}
```

### for
```
for ([init_expr] ; <condition_expr:bool> ; [iteration_expr])
{
  <NodeBlock>
}
```


## Comments
```
# single line
/#
multiple
lines
#/
```

## Containers

Sequence:
- All elements must have the same type
- Fixed and variable-sized (`array` / `list`)
  - Only if there's an advantage, otherwise only use `list`

> [!NOTE]
> Probably best to have `list` for now, implemented with a `std::vector`. 
>
> A cpy `list`'s implementation will hide behind an API, so the underlying container can be changed from `std::vector`/`std::list` if required.
>
> If `const` variables are supported, this can be `std::vector`. But an cpy `list` implemented as a `std::vector` may still be more performance than a `std::list` in many cases as it's unlikely users will still too many elements with frequent insertions/deletions.


Associative:
- Map

### List
- Extendable
- Denoted with `[` and `]`

```
list_ok := ["abc", "def", "ghi"];
list_ok.append("jkl");
```

### Array
- Fixed sized
- Denoted with `|`

```
array_ok := |"abc", "def", "ghi"|;
array_bork := |"abc", 42|;
```

### Map
- Extendable
- Denoted with `{` and `}`

```
contacts := {
  "john": ["12345", "16 Smith Drive"],
  "david": ["6789", "16 Park Street"]
}
```

### for each
Not ready for this, but something like:

```
for (<variable_decl_assign_expr> in <variable_ref:list|array>)
{

}
```
`variable_decl_assign_expr`:
- similar to `VariableDecl` and `VariableAssign` because it is declaring a variable and assigning in one expression.
- type is inferred from `<variable_ref>` (the element type of the `list`/`array`)


i.e.

```
some_array := |"abc", "def", "ghi"|;

for (a in some_array)
{

}
```
