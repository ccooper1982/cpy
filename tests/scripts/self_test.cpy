#! /home/callum/projects/cpy/build/cpy

/#
Use a cpy script to test cpy, marking its own homework.
#/

path := home()+"/projects/cpy/tests/scripts";
cpy := home()+"/projects/cpy/build/cpy";

script := path + "/sequence.cpy";

pipeline := proc(cpy, script);

result := to_int(exec_stdout(exec(pipeline)));

check(19, result);

fn check(expected: int, result: int)
{
    if (expected == result) {
        println ("PASS: ", result);
    }
    else {
        println ("FAIL: ", result);
        exit(1);
    }
}
