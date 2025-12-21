**Specification for the Basin programming language.**

# Table of Contents

(numbering is messed up, it's work in progress)

1. Introduction
   - Overview of the Basin language
   - Goals and design philosophy
   - Target audience and use cases

2. Programs and Source Code
   - Structure of a Basin program
   - File organization and conventions
   - Entry points

3. Lexical Structure
   - Allowed characters and encoding
   - Tokens: keywords, identifiers, literals, operators
   - Comments and whitespace rules

4. Syntax
   - Grammar rules
   - Statements and expressions
   - Blocks and scoping rules

5. Types
   - Primitive types
   - Composite types
   - Type inference and type checking
   - Type modifiers

9. Control Flow
   - Conditionals
   - Loops
   - Early exits

6. Functions
   - Function declarations and definitions
   - Function signatures and overloading
   - Return types and void functions
   - Anonymous functions or lambdas

7. Modules and Imports
   - Declaring and using modules
   - Importing libraries and resolving file paths
   - Scoping and visibility of imports

8. External Functions and Libraries
   - Syntax for declaring external functions
   - Linking external libraries
   - Error handling for unresolved symbols
   - Examples of using system libraries

10. Memory Management
    - Stack vs. heap allocation
    - Manual memory management

11. Error Handling
    - Compile-time errors
    - Runtime errors and exceptions
    - Debugging and error messages

12. Concurrency
    - Threads, coroutines, or async/await
    - Synchronization primitives

13. Standard Library
    - Overview of the standard library
    - Common modules
    - Guidelines for extending the standard library

14. Tooling and Compilation
    - Compiler behavior and flags
    - Build systems and workflows
    - Debugging and profiling tools

15. Intermediate Representation
   - Bytecode
   - 

<!-- 15. Examples
    - Simple "Hello, World!" program
    - Examples of common patterns and idioms
    - Advanced examples -->

<!-- 
16. Future Directions
    - Planned features and extensions
    - Backward compatibility considerations

17. Appendices
    - Formal grammar
    - Reserved keywords
    - Error codes and messages -->


# 1. Introduction
The Basin programming language is a complete rewrite of BetterThanBatch, an experimental language and compiler.

The Basin language is inspired by Jai, Odin, C, Rust.

The Goal of Basin is:
- Fast and simple to compile. 1 million lines per second (with multithreading).
- A straight forward type system.
- More similar to C than Rust.
- Execute basin programs (duh)
- Compile basin programs (to libraries and object files)
- Compile and distribute basin programs (executables)


## Requirements language

## Requirements compiler
A little strange to specify compiler specifics in "Basin Language Specification" but here they are.

- Small codebase, 30K - 50K lines (if you can call that small)


<!-- 
   - Overview of the Basin language
   - Goals and design philosophy
   - Target audience and use cases -->

# 2. Programs and Source Code

A Basin program begins from one initial source file. From there files can be imported, libraries to link with can be specified, and compiler options can be set inside the Basin language. This means that codebases using Basin alone will usually not use a separate build system such as Make, CMake, Premake, bash or python scripts.

The conventional file extension for basin source files is `.bsn`.

A basin program can be executed directly but can also produce executables to run and distribute. A program with code at the top level will be executed at compile time The entry point for programs will be the top-level expression in the initial source file. If a function is marked with `@entrypoint` then that function will be the entry point. This allows you to have the entry point in a different file than the initial source file. It also means you have access to `argc` and `argv` you would get from `main` in a C program.

A program can also be compiled as an object file or library in which case global variables and functions can be exported and visible to programs using the library. Functions not exported will not have symbols. Exported and external functions will automatically use the operating system's calling convention rather than Basin's.

The compiler supports these object file and executable formats: COFF (windows), ELF (linux), DWARF (gdb, MinGW on windows).

To allow multiple return values, Basin uses its own calling convention which is very similar to System V ABI. See [Basin Calling Convention] for more information.

When compiling intermediate files such as inline assembly and object files they will be placed in the `bin/int` directory. Executables, static and dynamic libraries will be placed in `bin` directly.

**NOTE**: There are plans to support .PDB which is debug information for Windows.

# 3. Lexical Structure
The lexical structure of Basin program defines the text encoding and which characters are allowed.

Basin source files should be encoded using UTF-8.

Lexical analysis is applied to a file where sequences of characters are converted to tokens. These are the tokens:
## Identifiers
Identifiers are names for variables, functions, and parameters. They start with a letter or underscore followed by letters, digits, and underscores. The allowed characters for letters are the English characters A-Z. Identifiers are sensitive to lower and upper case.

**Examples:**
- `myvar`
- `_secret`
- `_123_hey`
- `gold` and `Gold` are different identifiers.
- `41kopwa` not allowed
- `4_23` not allowed
- `char_ä` not allowed

## Keywords
Keywords are reserved identifiers.

Some are: `if`, `while`, `import`, `return`.

See `TokenKind` in [lexer.h](/src/basin/core/lexer.h).

(**TODO:** Once compiler implementation is more complete the full list will be put here)

## Literals
There are literal numbers, strings, characters, and booleans. They have specific values.

**Literal numbers:**
- `123`
- `12.244`
- `12.244e5` same as (12.244 * pow(10, 5))
- `0x123` hexidecimal
- `0o7267` octal
- `0b10011` binary

**Literal characters**
- `'a'` 
- `'\0'` 
- `'\n'`

**Literal strings**
- `"hello"`
- `"\"hello\""`
- `"'hello' is here"`
- `"hello\nhello2`

**Formatted strings**
- `f"Hello {num}\n"` -> "Hello ", num, "\n"
- `f"Hello {num}"` -> "Hello ", num, "\n"
```python
f"{x}{y}"     ->  x, y
f"a{x}{y}"    ->  "a", x, y
f"{x}a{y}"    ->  x, "a", y
f"{x}{y}a"    ->  x, y, "a"
f"{x}a{y}a"   ->  x, "a", y, "a"
f"a{x}a{y}"   ->  "a", x, "a", y
f"a{x}a{y}a"  ->  "a", x, "a", y, "a"
```

- `f"Hello {name:14}"` padding is not supported yet.

## Special characters
Operators and delimiters:
```
+-*/%^&<>~!
=
#@$
()[]{}
,.
```

## Whitespace
Whitespace is mostly ignored except for a few instances described in [Syntax](/docs/language/Basin%20Language%20Specification.md#4.%20Syntax).

Some operations in the language share special characters and whitespace is used to resolve the ambiguity.

One instance is to resolve operator kind:
- `variable&` means `ADDRESS OF variable`. ADDRESS OF is determined by no whitespace preceding the operator token.
- `variable & MASK` means `bitwise AND operation on variable and MASK`
- `variable& & MASK` means `bitwise AND operation on ADDRESS OF variable and MASK`
- `variable & & MASK` is invalid syntax. Two XOR operations next to each other is not allowed since it's a binary operation (takes two inputs on left and right side).

## Comments
```
// one line
/*
   multi-line
*/
```

# 4. Syntax

The primitive component of a Basin program is the expression. An expression can be an arithmetic operation (like addition), a function call, a literal value, an if-statement, a for-loop, a block containing more expressions. An expression is a value or a thing that can be executed.

Files themselves are a big block expression that contains more expressions. In a block expression there may also be global variables, constants, functions, structs, and enums. These are declarations and definitions and not expressions because they aren't executed. The initial value of a variable, constant or the code inside a function is an expression but not the entire function itself.

Below are some pseudo grammar rules:
- `thing` a normal name specifies a non-terminal, it expands to other non-terminals and terminals
- `thing | other_thing` allows one or another thing
- `("import" | "include") file_path` parenthesis declares precedence
- `"import"` a terminal, specifies a specific string of text (keyword)
- ` number ( "," number ) * ` asterisk specifies zero or more of the thing before
- ` number ( "+" number ) + ` plus specifies one or more of the thing before
- White space is ignored

```bash
file := block_expression

block_expression :=  ( expression | global | constant | function | struct | enum | import ) *

expression := ( "{" block_expression "}" ) | loop_expression | if_expression | identifier | literal_expression | math_operation | function_call | assignment | local_var_declaration

# First expression is a range or slice
#   if slice then first identifier will be the item, second identifier will be index
#   if range then first identifier will be index, second identifier not allowed
loop_expression := "for" identifier "in" expression  expression
loop_expression := "for" identifier, identifier "in" expression  expression
loop_expression := "for" expression  expression

# conditional expression that should evaluate to a boolean, keep looping while true
loop_expression := "while" expression expression
# loop forever (until break or return)
loop_expression := "while" "{" ( expression | block_expression )  "}"
```

# 5. Types

Types defined by the language are lowercase including `string`.

## Primitive types
```
// integers
u8, u16, u32, u64 // 1,2,4,8 bytes
s8, s16, s32, s64 // 1,2,4,8 bytes

// floating point (IEEE)
f32, f64 // 4,8 bytes

// other
bool, char // 1,1 bytes

// pointers
void^
i32^
char^

// function pointers
fn()
fn(i32)->i32

// arrays with fixed size
i32[8]
char[255]

// slice (pointer + length)
char[]

// types with platform dependent size
uword, sword     // same size as the CPU's register size
```

## Enum types

```
enum Ore {
   iron,
   coal,
   gold,
}
```

## Struct types

```
struct Block  {
   valid: bool
   len: u32
   max: u32
   data: u8^
}
```

### Bit fields

```rust
struct Register {
   enable : u8
   _reserved: u8[3]
   data: u32
}
```

## Polymorphic structs

```rust
struct Array<T> {
    len: i32
    cap: i32
    data: T^
}

struct FixedArray<T,N> {
    len: i32
    cap: i32
    data: T[N]
}
```


## Strings
The language has a builtin string type.

The classic `string` is a so called `builder` where you can slice and concatenate strings. Memory is allocated from an `allocator` present as a pointer field in the string struct. You may override this allocator or use the default heap allocator.

Strings use UTF-8 encoding. However, and index and the length to the character data is byte-based, not rune based. `myString[1]` gets you the second byte in the utf-8 string, not the second rune. `import "string"` provides extra functions for utf-8, split, and path join.

A string is implemented as a struct with these fields:
```rust
struct string {
   ptr: char*
   len: u32
   cap: u32
   allocator: Allocator*
}
// 24 bytes on a 64-bit system
```

This string type is convenient in many use cases. For cases where it isn't optimal you can always make your own string type. Most string operations use character slices (`char[]`) which means that your string implementation can integrate with those operations.

## Operations
Operations can be divided into mutable and immutable. Immutable operations do not allocate memory or change the content of the string. Mutable operations do.

### Comparison (immutable)
```c
a, b : string

a == b
a != b
```

### Slicing (immutable)
```c
a : string

a[5]     // yields char
a[1..4]  // yields char[]
```

### Concatenation (mutable)
```c
res, a, b : string

res  := a + b
res  := a + 'a'
res  := a + "hello"
```


We want to support concatenating two literal strings.
Concateting two variable slices

```
ROOT_PATH : char[] = "assets"   // default
NAME : char[] = "settings.txt"  // normal

res: string = ROOT_PATH + "/" + NAME
```

In embedded we don't have heap. char[] + char[] can't become string where default
heap is used. if the context have an allocator then it's fine?

if we export a function which concatenates strings then there is no context.
(C doesn't have Basin context, when it calls Basin functions there also is no context). you can pass or put context in thread local storage, then we have context.

so no string concatenation for char[]+char[] in exported functions? put string on stack? what if it's huge text? don't know that until runtime. fixed size strings by default. mixing fixed strings and heap strings is akward.

basin runtime has a global variable that declares default allocator for strings.
can be overriden if you wish. char[] + char[] will use that?

you may want strings to use a special allocator while other memory (arrays) uses a different allocator.

i32[.]

@operator fn __cast<char[]>(arg: string) -> char[] {

}

operator cast<char[]>(arg: string) -> char[] {

}

### Content modification (mutable)
```c
a : string = "hello"

a[1] = '4'
```


Const operations you perform on a string take character slices (`char[]`)
 and not optimal where performance
is critical. For those cases it is encouraged to allocate your own memoeruse your own string type or 

i32*.bytes
.bits
nbytes
nbits


## Assignment


```
// infer
hello := 214

//
okay := ( hello := 123;
         yield hello + 23 )

```



# 15. Intermediate Representation

While the Basin language doesn't require bytecode it makes sense
to implement it for the compiler internals. Running bytecode instead of
generating snippets of x86 or ARM (depending on host architecture) gives
the compiler intricate control over the addresses that can and can't be
accessed. Call IR instruction can easily be wrapped to call compiler functions,
dynamic libraries (GLFW, OpenSSL), or plugins.

## IR syntax
Because the IR is an internal component in the compiler it doesn't
need a syntax. However, it is convenient to have a way to describe
IR in text for testing.

### General Layout
HM, we may need to handle struct types.
Calling convention behaves differently if you have 16-byte struct vs larger.
(if 16 then two 8-byte integers go into registers)

```

variable <name> <type> <>

routine <name> <signature> <argument SSAs> <body>

routine @hello () {

<outputs...> = call <function> <inputs...>

<result> = add <type>, <operand_0>, <operand_1>

%1, %2 = call hello, %3, %4

call_reg %9, %1, %2

}
```

## 