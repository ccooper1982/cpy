# cpy
C++ Pythony

An alternative to shell script.

# Build
```
cd grammar && tree-sitter generate
cd ..
meson setup build
meson compile -C build
```

# Features
- Statically typed
- Type system has knowledge of procceses, files, etc

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

Find all log entries starting "DEBUG" then print the line count (`lcount`) to standard out: 
```
result := grep("log.txt", "^\[DEBUG\]") | lcount()
sout << result;
```

Find "DEBUG" entries, split on ' ', returning from second token onwards:
```
result := grep("log.txt", "^\[DEBUG\]") | split(' ', 2)
sout << result;
```

Equivalent to: `grep '^\[DEBUG\]' abc.txt | cut -d' ' -f2-`

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
