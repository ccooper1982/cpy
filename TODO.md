# TODO

## env
Access environment variables:
- `env(str: name)` : returns value with `std::getenv()`
- `env_set(str: name, str: val)`

## filesystem
- `file_create(path: str, overwrite: bool = false)`
- `file_create_sym(src: str, target: str, overwrite: bool = false)`
- `file_rmv(path: str, recurse: bool = true)`
- `file_set_perms(path: str, perms: int)` - set permissions
- `file_perms(path: str) -> int` - get permissions
- `file_is_dir(path: str) -> bool` 
- `dir_list(path: str)` - list of files in dir (not recursive)



### for each
```
for (<variable_decl_assign_expr> in <variable:array>)
{
  <NodeBlock>
}
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

`variable_decl_assign_expr`:
- similar to `VariableDecl` and `VariableAssign` because it is declaring a variable and assigning in one expression
- type is inferred from `<variable_ref>` (the element type of the `list`/`map`)


list:
```
my_files = file_list("some/dir");
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
