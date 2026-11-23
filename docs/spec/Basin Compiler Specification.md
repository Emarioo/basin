**Specification for the Basin compiler**

Usage details about the compiler.
Some aspects of interests.

# Requirements

## The same input generates the same output

The same 10 000 lines of code compiled with the same parameters to the same compiler should
produce the same binary (object file, executable, library).

The exception would be if you have compile time execution that gets a date and builds it into the
binary on each rebuild.

# Recommended architecture decisions

## Multi-threading and task system
Use multiple threads with a task system where each threads pick a task (parse, type check, code gen) until no more tasks exist.


# CLI arguments

See `basin_parse_argv()` in [basin.c](../../src/basin/basin.c)

# Library parameters

See `BasinCompileOptions` in [basin.h](../../include/basin/basin.h)
