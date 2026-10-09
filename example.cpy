fn foo1(a: int) -> int {}
fn foo2(a: int, b: str) -> int {}

a: int;
b: str;

foo1(a+a);
foo2(foo1(a), "abc");
foo2(foo1(a), b);
foo1(a+b);
