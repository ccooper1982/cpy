
fn test1()
{
    pipeline := proc("find", home()+"/projects/shl", "-type", "f", "-name", "*.cpp")
              | proc("wc", "-l");

    result := exec(pipeline);

    if (exit_status(result) == 0) {
        println("C++ files found: ", result);
    }
    else {
        println("Failed to count C++ files");
    }
}


fn test2()
{
    pipeline := proc("seq", 1, 100000)
            | proc("grep", 5)
            | proc("sort", "-r")
            | proc("wc", "-l");

    result := exec(pipeline);

    println("Exit status: ", exit_status(result));
    println("Count: ", result);
}


fn test3()
{
    pipeline := proc("printf", "hello\nworld\n")
            | proc("grep", "banana");

    result := exec(pipeline);

    println("Exit status: ", exit_status(result));
    println("Output: [", result, "]");
}


fn test4()
{
    pipeline := proc("seq", "1", "100000")
            | proc("grep", "5")
            | proc("wc", "-l");

    result := exec(pipeline);

    println("Exit status: ", exit_status(result));
    println("Numbers containing 5: ", result);
}

i := 4;
d := 4.5;
b := false;

println(to_str(i));
println(to_str(d));
println(to_str(b));

println(to_dec(i));

println(to_bool("true"));

println(env("SHELL"));

/#
test1();
test2();
test3();
test4();
#/
