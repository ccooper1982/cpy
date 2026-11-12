# cpy
C++ Pythony, an alternative to shell script.

Mostly experimenting with Tree-sitter and creating an AST.

```
p1 := proc("cat", "/etc/hosts");
p1_result := exec(p1);

println ("Result: ", exit_status(p1_result));

if (exit_status(p1_result) == 0) {
    println (p1_result);
}
else {
    println ("It failed");
}
```

# Aim
- A programmer friendly scripting language
- No config files or virtual environment required
- Integrated knowledge of pipes and processes
- No memory management
- Type safety


# Build
```
cd grammar && tree-sitter generate
cd ..
meson setup build
meson compile -C build
```

# Test
```
meson test -C build
```

---

# TODO

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

```
home := env_get("HOME");
env_create("BUILD_DIR", home + "/projects/blah/build");
```
