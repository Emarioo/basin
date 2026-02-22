
# Compile time performance

Our target is: `1 000 000 lines / second`

We give ourselves some leeway to use modern CPU (Ryzen 9 9950x, 32 threads, ~4.4 GHz base, 5.7 GHz boost).

If we achive the target on Windows or Linux but not both then it is still a success.

We measure from calling the compile function to returning from it. Time Windows/Linux takes to start the process is not included.

Some math:
```
31250 = 1000000 / 32
31.25 lines / millisecond
```

Whole compilation cannot be parallelized, generating object file for example. So a little less than `31 lines per / ms` is what
we need to achieve.

The flags to use is debug info but no optimizations.

Whether we achieve it or not depends on the source code we compile.
Writing 1 million lines will take a while. Writing 50 000 lines and expecting it to take 50 ms is doable but not accurate.
With 1 million lines we have many nodes, much machine code, much comp time execution, much debug info which we won't get with just
50 000 lines. CPU cache is hit different and the workload for 32 threads is different with 1 000 000 lines and very interesting.

We can generate 1 million lines but we need to do so in a way that somewhat represents a real codebase.

- 1 million variables, functions per line is not interesting.
- Avoiding comp time to achieve `1 million lines / s` is not interesting.
- 1 million lines in one file is interesting but we also want to spread it unevenly across multiple files.

## Stats for generated code

Our generated code has some metrics we can choose:
- Number of functions
- Number of global variables
- Constants
- Number of files
- Number of structs, enums
- Number of imports

On the function body level

- Number of if statements, for loops
- Number of expressions
- Number operations, function calls, literals
