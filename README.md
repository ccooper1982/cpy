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

# Tests
```
meson test -C build
```

# Ideas
- Statically typed
- Language has knowledge of procceses, files, etc

## Modules
- file
  - search, exist, create, delete, etc
- env (environment vars)
  - exist, set, search
- proc (processes)
  - run, pipe, redirect output

Access with `<module_name>::<function>`, i.e. `file::exist("foo.txt")`

## Files
- exists
- size
- permissions
- create
- delete
- copy
- move

```
dir := file::list("some/dir");
for (file in dir) {
  cout << file.name() << " @ " << file.size() << " bytes";
}

file::create("some/file.txt");
```

## Control Flow
- conditional: `if`
- loops: `for`, `while`
- return: `return`, `return_if`

```
if (!file::exist("some/file.txt")) {
  return_if !file::create("some/file.txt");

  return parse_file("some/file.txt");
}
```

## Processes
- Execution
- Exit code
- stdout, stderr
- redirection

```
grep_debug := proc::grep("log.txt", "^\[DEBUG\]");
cout << grep_debug | proc::line_count;
```

```
grep_debug := proc::grep("log.txt", "^\[DEBUG\]");
grep_result := grep_debug.run();
file::create("result.txt", grep_result);
```

## Environment Vars
- Get
- Set
- Search

```
home := env::get("HOME");
env::create("BUILD_DIR", home + "/projects/blah/build");
```
