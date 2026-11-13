
pipeline := proc("seq", 1, 100000)
          | proc("grep", 5)
          | proc("sort", "-r")
          | proc("wc", "-l");

result := exec(pipeline);

println("Exit status: ", exit_status(result));
println("Count: ", result);


/#
pipeline := proc("printf", "hello\nworld\n")
          | proc("grep", "banana");

result := exec(pipeline);

println("Exit status: ", exit_status(result));
println("Output: [", result, "]");
#/


/#
pipeline := proc("seq", "1", "100000")
          | proc("grep", "5")
          | proc("wc", "-l");

result := exec(pipeline);

println("Exit status: ", exit_status(result));
println("Numbers containing 5: ", result);
#/

/#
pipeline := proc("find", "~/projects/cpy", "-type", "f", "-name", "*.cpp")
          | proc("wc", "-l");

result := exec(pipeline);

if (exit_status(result) == 0) {
    println("C++ files found: ", result);
}
else {
    println("Failed to count C++ files");
}
#/
