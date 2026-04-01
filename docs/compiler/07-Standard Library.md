How to do a good standard library.

**Who:** I will make it. It can be used by those who install the Basin compiler.
It isn't meant for developers writing in other languages to use it. stdlib must be compiled as a
library and then you must provide declarations for it (Basin compiler provides a way to generate C headers with declarations).
It's possible but not our intention.

**What:** Standard library consists of programs that help developers make their projects. For example:
- pre-made bindings for libraries and operating systems (GLFW, STB image, Windows, Linux, OpenSSL)
- Memory allocators
- String manipulation functions
- Platform independent layer for file system, threads, networking, processes, date and time
- Math
- Common data structures, dynamic arrays, hash maps, bucket arrays.

**Why**
You can write these yourself for every project but it's often unnecessary.
It's also convenient if everyone uses the same string type from the standard library in their APIs.
Better compatibility in the library ecosystem.

**Where:** Located in the Basin Compiler repository. Is all of standard library there?
For example some more external less important ones like JSON, HTTP module may exist separately?
I believe it's good to keep everything in one repository. However we'll keep core stdlib modules (memory allocators, math) and
extra JSON, HTTP modules separate.


**When:** Working on the stdlib right now. I need to decide whether dynamic arrays, maps and strings are builtin
types or exist in the standard library. I'm leaning towards standard library since they undoubtedly require memory allocations.
I want users to have control over their memory. That's where custom allocators come in. Users can override allocators.
I want language and compiler to be simple, introducing strings, maps, arrays in the compiler will create complexity. Which is fine
if it's worth.

Slices and ranges are builtin types. A personal note: BTB had Range and polymorphic Slice struct defined in preload.btb which was always
imported. In basin I believe these won't be structs but their own types. Layout-wise they are structs with two fields.

It is done before 1.0.0. Or rather, 1.0.0 can't be released until stdlib is finished and stable.

**How:** Implement stdlib as i go. All things i plan on adding doesn't have to exist in 1.0.0.
For example sound, graphics can be added later in 1.0.1 but core stuff, math, hash maps, memory allocators
need to exist.


# P roblems to solve

## 1. Duplicated stdlib implementations from every Basin library.

Every basin library will contain implementation for stdlib because `import "fs"` will define the file functions and their implementation.
When producing object file and static library those functions will exist in the library and when you link multiple Basin compiled libraries
they will have duplicated stdlib implementation.

**Problem:** Duplicated stdlib which means larger binary size. If you have 10 basin libraries you link with then that's a lot of duplicated code.

**Why it's not a big problem:** The libraries may be compiled with different Basin version which means different stdlib versions. They may have incompatiblities in the API
which means a stdlib implementation for each Basin library is necessary. For example: On Linux you may have a version of some shared library which is used by a program and then you have another program that is old and uses an older version of the shared library. The API for the shared library is incompatible which each other and the two programs must use a specific version of the shared library or things will break.

For now the stdlib is compiled without `@export`. This would be the `static` keyword in C terminlogy. This means there won't be any name collisions if two basin libraries are linked where both use Basin stdlib.

**Solution:**
- We need to measure how much extra code the stdlib adds to libraries and whether we deem the duplicated code to be a problem. If it's like 1KB then we're totally fine. The GLFW shared library is around ~500 KB. If you have a library where 2% is duplicated stdlib then maybe that's not a big deal. Assuming you just use portion of stdlib which is 1 KB. Probably bigger than this in practise which is why we need to measure.
- If it's a big deal (mainly when linking 10 basin libraries) then we should provide a way to compile the stdlib as `basinstd.lib` which you can link with. This also means that all functions in the stdlib needs two versions. One which is statically compiled (no export or import), a second with `@export` (when we compile `basinstd.lib`), a third where all declarations have `@import` (used by libraries that want to link with the static library instead of compiling and including stdlib implementation in the user library).

Having the code below for every declaration is a nightmare to read and maintain. For all stdlib functions we have to remember to do this.
And for some we don't want to export.
```c
// Basin doesn't have a conditional compilation or preprocessor like this.
// Code is a mix between Basin and C.

#if defined(EXPORT_STDLIB)
    
    @export
    read_whole_file :: (path: string) -> string { /* ... */ }

#elif defined(IMPORT_STDLIB)
    
    library "basinstd.lib" as BASIN_STDLIB
    @import(BASIN_STDLIB)
    read_whole_file :: (path: string) -> string

#else
    // Statically compiled
    read_whole_file :: (path: string) -> string { /* ... */ }
#endif
```

An imported code file and a code file compiled into a library are the same. The syntax, keywords, annotations does not change.

Developer makes a module with "play_sound" function. Developers import the module with import "audio.bsn" and use it in code.
They simply compile with "basin main.bsn". The play_sound function is compiled statically into the object file.

Developers want to use the module as a static library. They import "audio-decl.bsn". The decl file is distributed with the static library. Decl file is
generated when compiling the static library.

Developer of play_sound must specify which functions in the module should be visible externally when compiled as static library.
This is done with `@export`. However, now whenever you just import the module it will get exported symbols. This is not fine if multiple
compiled libraries import it because there will be duplciate symbols.

