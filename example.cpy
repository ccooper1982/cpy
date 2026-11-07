
println (argv_str(0));
println (argv_str(1));
println (argv_int(2));
println (argv_dec(3));
println (argv_bool(4));
println (argv_bool(5));

if (argv_bool(4)) {
    println("yes");
}

println(argv_int(2) == 1);
