fn foo(x: ref str)
{
    bar(ref x);
}

fn bar(xx: ref str)
{
    xx = "world";
}

a := "hello";

foo(ref a);
