fn foo1(a: int, b: str) -> int {}
fn foo2() -> int {}
fn foo3() -> str {}
fn foo4(a: int, b: str, c: int) -> int {}

a := foo4(foo2(), foo3(), foo1(foo2(), foo3()));
