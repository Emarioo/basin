**Specification for the Basin programming language**

This document contains all you need to write programs for the language.
What is not covered is the specific arguments you pass to the compiler.

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
   - Functions
   - Structures
   - Enums
   - Gloabls
   - Constants
   - Imports
   - Expressions
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
- Meant for software where safety and security is not critical.
- Execute basin programs
- Compile basin programs (to libraries and object files)
- Compile and distribute basin programs (executables)

meh goals


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

Basin source files are encoded as a stream of UTF-8 characters.

The characters can be considered as a stream of tokens where each tokens is one of the following:
- Identifier
- Integer
- Decimal
- String
- Rune
- Special character

The following can exist betwene tokens:
- Whitespace
- Comments

## Identifier
Identifiers are names for variables, functions, parameters, keywords, and type names. They start with a letter or underscore followed by letters, digits, and underscores. Identifiers are case-sensitive.

**Examples:**
- `myvar`
- `_secret`
- `_123_hey`
- `gold` and `Gold` refer to different things.

**Not allowed:**
- `41kopwa`
- `4_23`
- `char_ä`

**Unicode characters:**
- **TODO**

## Integer
- `123`
- `0x123` hexidecimal
- `0o7267` octal
- `0b10_0011` binary

## Decimal
- `12.244`
- `12.244e5` same as (12.244 * pow(10, 5))

## String
**Literal strings**
- `"hello"`
- `"\"hello\""`
- `"'hello' is here"`
- `"hello\nhello2`

**Escape sequences**
- `"\0"`
- `"\t"`
- `"\n"`
- `"\r"`
- `"\e"` (0x1b)
- `"\""` (")
- `"\'"` (')
- `"\x41"` (byte A)
- `"\u03BB"` (Unicode -> UTF-8, λ)
- `"\U000003bb"` (Unicode -> UTF-8, λ)

**Formatted strings (WIP)**
- `f"Hello {num}\n"`
- `f"package/game-{version}-{target}"`

<!--
not translated to comma like this, wont' work with a := f"package-{version}"
 -> "Hello ", num, "\n"
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
-->

## Rune
**Literal characters**
- `'a'` 
- `'\0'` 
- `'\n'`
- `'\x41'`


## Special characters
Operators and delimiters:
```
+-*/%
^&<>~!
=#@$
()[]{}
,.;:
```

## Whitespace
Whitespace is mostly ignored except for a few instances described in [Syntax](./Basin%20Language%20Specification.md#4.%20Syntax).

Some operations in the language share special characters and whitespace is used to resolve the ambiguity. It makes things interesting...

Example of whitespace solving ambiguity:
- `variable&` means `ADDRESS OF variable`. ADDRESS OF is determined by no whitespace preceding the operator token.
- `variable & MASK` means `bitwise AND operation on variable and MASK`
- `variable& & MASK` means `bitwise AND operation on ADDRESS OF variable and MASK`
- `variable & & MASK` is invalid syntax. Two bitwise AND operations next to each other is not allowed since it's a binary operation (takes two inputs on left and right side).

## Comments
```
// one line
/*
   multi-line
*/
/*
   Hello
   /*
      They can be nested
   */
   Still a comment
*/
```

# 4. Syntax

The syntax of Basin revolves around expressions. An expression can be an arithmetic operation (like addition), a function call, a literal value, an if-statement, a for-loop, a block containing more expressions. An expression can be seen as something that executes.

The text in a file is considered contents of a block expression that contains more expressions. In a block expression there may be global variables, constants, functions, structures, enums, and imports. These are declarations and definitions and not expressions because they are not executed. The initial value of a variable, constant or the code inside a function is an expression but not the declaration of the function or constant itself.

EBNF grammar has not been written here because there is ambiguity in the language and whitespace matters in certain instances.

Below are some pseudo grammar rules:
|Rule|Description|
|-|-|
|`IDENTIFIER`                          | A terminal that specifies any identifier|
|`"import"`                            | A terminal that specifies a specific identifier|
|`"+"`                                 | A terminal that specifies a special character|
|`INTEGER`                             | A terminal that specifies an integer|
|`DECIMAL`                             | A terminal that specifies a decimal|
|`CHARACTER`                           | A terminal that specifies an integer|
|`STRING`                              | A terminal that specifies a string|
|`node`                                | A non-terminal which expands to other non-terminals or terminals|
|`node \| other_node`                  | Allows one of the nodes|
|`( "import" \| "include" ) file_path` | Parenthesis declares precedence. This without parenthesis `"import" \| "include" file_path` would mean "import" **OR** "include <file_path>"|
|` number ( "," number ) * `           | Asterisk specifies zero or more of the preceding node|
|` number ( "+" number ) + `           | Plus specifies one or more of the preceding node|
|` number ( "+" number ) ? `           | Question mark specifies zero or one of the preceding node|

The following grammar describe the full syntax. See [Operations](./Basin%20Language%20Specification.md#6.%20Operations) for practical examples.

```bash
file := block_expression

block_expression := ( expression | global | constant | function | struct | enum | import )*

expression := \
    ( "{" block_expression "}" ) |
    loop_expression              |
    if_expression                |
    switch_expression            |
    function_call                |
    literal_expression           |
    math_operation               |
    local_var_declaration        |
    assignment                   |
    function_def                 |
    struct_def                   |
    enum_def                     |
    IDENTIFIER                   

loop_expression := \
    ( "for" IDENTIFIER, IDENTIFIER "in" expression  expression ) |
    ( "for" IDENTIFIER "in" expression  expression )             |
    ( "for" expression  expression )

if_expression := \
    "if" expression  expression  ( "else" expression ) ?

switch_expression := \
    "switch" expression "{" case * default_case ? "}"

case         := "case" expression expression *
default_case := "default" expression *

function_call := \
    identifier "(" argument_list ? ")"

argument_list := argument ( "," argument ) *
argument      := ( expression | IDENTIFIER "=" expression )

function_def := \
    "fn" IDENTIFIER 

# Annotations are not declared in any node above because any number of them can be added on any node above.
# @TODO Clearly describe which nodes support annotations.
annotation := "@" IDENTIFIER "(" argument_list ? ")"

```


@TODO I have to solve this:

```c
// normally {} represent initializers
v := struct16{ ptr0, ptr1 }

// in the language {} is just for code blocks.
// [] represent index operator or initializer.
// This is however ambiguous with index operator.
v := struct16[ ptr0, ptr1 ]

// i guest we can just cast operator
v := [ ptr0, ptr1 ] : struct16   // experimental cast operator, : is usually related to types, might make since. any ambiguouy though?
v := cast<struct16> [ ptr0, ptr1 ]

// we always have this as fallback
v: struct16 = [ ptr0, ptr1 ]

// Our index operator should allow for nice slicing.
v := struct16[ 0 : 5 ]  // this has ambiguouity with cast operator
v := struct16[ 0 : i32 : 5 ]  // this has ambiguouity with cast operator

// operator overloading
"[:]" :: (arr: i32[], x: i32, y : i32) -> i32[] {

}

v := struct16[0, 5]
"[,]" :: (arr: i32[], x: i32, y : i32) -> i32[] {

}

v := struct16[0, 5]
"[]" :: (arr: i32[], x: i32) -> i32 {

}

```

# 5. Types

The language defines the following types.

## Primitive types
```rust
// integers
u8, u16, u32, u64 // 1, 2, 4, 8 bytes
s8, s16, s32, s64 // 1, 2, 4, 8 bytes

// floating point (IEEE)
f32, f64 // 4, 8 bytes

// other
bool // 1 byte
char // 1 byte
rune // 4 bytes

// pointers
void*
i32*
char*

// function pointers
fn()
fn(i32)->i32

// arrays with fixed size
i32[8]
char[255]

// slice (pointer + length, the latter is uword)
char[]

// range (start + end, both are uword)
range

// types with platform dependent size
uword, sword     // 8 bytes on 64-bit Architecture
```

## Enum types

```rust
enum Ore {
   iron
   coal
   gold
}

enum Attribute : u8 {
    Unknown,
    Strong,
    Weak,
}
```

## Struct types

```rust
struct Block {
   valid: bool
   len: u32
   max: u32
   data: u8*
}
```

## Polymorphic structs

```rust
struct Array<T> {
    len: i32
    cap: i32
    data: T*
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

### Operations (string)
Operations can be divided into mutable and immutable. Immutable operations do not allocate memory or change the content of the string. Mutable operations do.

#### Comparison (immutable)
```c
a, b : string

a == b
a != b
```

#### Slicing (immutable)
```c
a : string

a[5]     // yields char
a[1..4]  // yields char[]
```

#### Concatenation (mutable)
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

#### Content modification (mutable)
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


# Operations

The following are the basic operations, most apply to scalar types (integers).
```c
A + B  (add)
A - B  (subtract)
-A     (subtract by zero)
A * B  (multiply)
A / B  (divide)
A % B  (modulo)
A %% B (positive modulo)
A ?? B (either or)
A | B  (bitwise or)
A & B  (bitwise and)
A << B (bitwise shift left)
A >> B (bitwise shift right)
A ^ B  (bitwise xor)
B~     (bitwise negate)
A!     (not)
A || B (or)
A && B (and)
A < B  (less than)
A > B  (greater than)
A == B (equal)
A <= B (less than or equal)
A >= B (greater than or requal)
A != B (not equal)
A[B]   (index if B is integer, slice if B is range)
A&     (address of)
A^     (dereference)
A: B   (type cast)
A: B   (declaration)
A := B (declaration and assignment)
A = B  (assignment)
..     (range)
.      (member)
```

Operator can be overloaded to different types.

There is ambiguity in the syntax above if you do not respect whitespace.


```
// infer
hello := 214

//
okay := ( hello := 123;
         yield hello + 23 )

```

# Control Flow

## If


```c
if <condition>
   <expression>
else
   <expression>
```

## Switch

```c
switch <condition> {
   case <condition>
   case <condition>
      <expression>
   case <condition>
      <expression>
      <expression>
   default
      <expression>
}
```

## For


 First expression is a range or slice<br>
   if slice then first identifier will be the item, second identifier will be index<br>
   if range then first identifier will be index, second identifier not allowed<br>

```c
for <expression>
   <expression>
for item in <expression>
   <expression>
for item, index in <expression>
   <expression>
```


## continue, break

```c
for <expression> {
   
   continue // continue loop
   
   break // break out of loop
}
```

## defer

```c
defer <expression>
```

# 7. Modules and Imports

Anyone who imports api.bsn will also get symbols from util.bsn if you use `@share`.
```c

// util.bsn

fn util_number() -> i32 { return 5 }

// api.bsn

@share
import "./util.bsn"

// main.bsn

import "./api.bsn"

print(util_number())

```

# 15. Intermediate Representation

@TODO Move to compiler specification

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