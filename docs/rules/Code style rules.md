When writing code in this project you must think about these:
- Multithreading
- Memory ownership
- Error handling, from function calls and how compiler error messages are propagated

You must do these things:
- Always use `string` from platform layer UNLESS it's for the Basin API (it should use C strings).

# Asserts
Asserts are used when it's very likely to go wrong. Such as:
- Indexing into an array
- Allocating memory
- Default in switch case (if new enum value is added, assert will trigger on the unhandled case)

These are always supposed to work.
