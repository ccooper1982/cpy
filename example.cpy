# p1 := proc("cat", "/etc/fstab");
p1 := proc("env");

println(p1);

println("Status: ", proc_run(p1));
println(proc_run(proc("true")));   # 0
println(proc_run(proc("false")));  # 1
