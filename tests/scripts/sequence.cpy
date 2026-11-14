#! /home/callum/projects/cpy/build/cpy

pipeline := proc("seq", "1", "100")
          | proc("grep", "5")
          | proc("wc", "-l");

result := exec(pipeline);

print(result);
