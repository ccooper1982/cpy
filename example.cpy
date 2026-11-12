p1 := proc("cat", "/etc/hosts");
p1_result := exec(p1);

println ("Result: ", exit_status(p1_result));

if (exit_status(p1_result) == 0) {
    println (p1_result);
}
else {
    println ("It failed");
}
