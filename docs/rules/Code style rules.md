When writing code in this project you must keep these in mind:
- Multithreading
- Memory ownership
- Massive source code files
- Error handling, from function calls and how compiler error messages are propagated
- Use `string` from util UNLESS it's for public API (it should use C strings).
- All platform specific code goes in `src/platform`. The compiler source code should be completely platform independent (`src/basin`, `src/util`).


# Asserts
Asserts are used when it's very likely to go wrong. Such as:
- Indexing into an array
- Allocating memory
- Default in switch case (if new enum value is added, assert will trigger on the unhandled case)

These are always supposed to work.
