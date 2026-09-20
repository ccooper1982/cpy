# cpy
C++ Pythony

An alternative shell script.

# Build
```
cd grammar && tree-sitter generate
cd ..
meson setup build
meson compile -C build
```

# Features
- Statically typed
- Type system has knowledge of procces, files, etc

# Filters
Similar to C++ `std::views`:

```
for (txt_file : files::list("dir1", ".txt") | files::filter((file: File){ return file.size() > 100; })) {
    sout << txt_file << "\n";
    files::delete(txt_file);
}
```

# Modules
- files
  - search, exist, create, delete, etc
- env (environment vars)
  - exist, set, search
- proc (processes)
  - run, pipe, redirect output

Module access with `<module_name>::<function>`, i.e. `files::exist("foo.txt")`

## Control Flow
- conditional: `if`
- loops: `for`, `while`
- return: `return`, `return_if`

## Processes
- Execution
- Exit code
- stdout, stderr
- redirection   

## Files
- exists
- size
- permissions
- create
- delete
- copy
- move

## Environment Vars
- Get
- Set
- Search
