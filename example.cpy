fn foo(n: int) -> int
{
    if (n == 0) {
        return 42;
    }

    while (true)
    {
        if (n > 0) {
            return foo(n - 1);
        }
    }

    return 99;
}

result := foo(5);

println(result);
println("Finished");
