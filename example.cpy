    fn foo(a: int) -> int
    {
      return a*2;
    }

    a :bool = false;
    b :int = 0;
    c :int = 0;
    z :int = 0;

    if (a) {
      z = foo(1);
    }
    else if (b == c) {
      z = foo(2);
    }
    else if (b > c) {
      z = foo(3);
    }
    else {
      z = foo(4);
    }
