# TODO

## Parameter Semantics
```
fn foo (x: int)
{
  x = 3;
}
```

`x` is pased by value, do we want to support pass by reference? Probably useful.

What is syntax:

```
fn foo (x: ref int)
{
  x = 3;
}

a: int;
foo(ref a);
```

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

### for each

Not ready for this, but something like:
```
for (<assign_expr> : <condition_expr:bool>)
{

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
