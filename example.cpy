fn foo1(a: int) -> int {}
fn foo2(a: int) -> int {}
fn foo3(a: str) -> int {}

a := foo2(foo1(1));
b := foo2(foo1(foo3("hello")));
