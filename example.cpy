
fn get_x() -> int
{
    return 5;
}

a := true;
i := 0;

while (a && i < get_x()) {
    println (i);
    i = i+1;
}
