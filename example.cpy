p1 := proc("hello", "world");
p2 := proc("hello2", "world2");
p3 := proc("hello3", "world3");

println(p1);
println(p2);
println(p3);

pipeline1 := p1 | p2;
pipeline2 := p3 | pipeline1 ;
pipeline3 := pipeline1 | p3;
pipeline4 := pipeline2 | pipeline3;

println(pipeline1);
println(pipeline2);
println(pipeline3);
println(pipeline4);
