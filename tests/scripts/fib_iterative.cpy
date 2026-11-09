fn fibonacci(n: int) -> int
{
    if (n <= 1) {
        return n;
    }

    a := 0;
    b := 1;
    i := 2;

    while (i <= n)
    {
        next := a + b;
        a = b;
        b = next;
        i = i + 1;
    }

    return b;
}

exit(fibonacci(arg_int(0)));
