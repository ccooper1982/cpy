fn foo1(a: int, b: str) -> int {}
fn foo2() -> int {}
fn foo3() -> str {}

a := foo1(foo2(), foo3());
