These are features we need to bake into the specifications.

Be prepared for some very unstructured requirements. It's more like ideas and needs.

# Conditional compilation

A devloper wants to create a platform library for their application.
To do so they need to reference external functions from different shared libraries.
Or write inline assembly and make the syscall directly.

This can be done with conditional compilation where we generate a piece of code
depending on the target operating system.

An alternative is having suffixes for files like platform_windows.bsn and platform_linux.bsn
where the compiler imports the respective file for the OS we are targeting.

Conditional compilation is not needed in that case.

```odin

when windows {
    
    library "libc.so" as libc

    @external(libc) fn WriteFile(handle: u64, buf: void*, count: uword, written: void*, overlapped: void*) -> uword
    @external(libc) fn GetStdHandle(handle: i32) -> u64

    fn main() -> i32 {
        text : char[] = "\u03bb\"Hello \x1b[31mWindows\"\e[0m\n"
        handle := GetStdHandle(-11)
        WriteFile(handle, text.ptr, text.len, null, null)
        return 5
    }

} else when linux {
    library "libc.so" as libc

    @external(libc) fn write(fd: i32, buf: void*, count: uword) -> uword

    fn main() -> i32 {
        text : char[] = "\u03bb\"Hello \x1b[31mLinux\"\e[0m\n"
        write(1 + 0, text.ptr, text.len)
        return 5
    }
}

```

If compiler doesn't support the specific OS you are targetting (my OS for example)
you can do the following manually. So we kind of have to support conditional imports.
Or at least there is a reason to want conditional import?

```odin

when windows {
    import "platform_windows.bsn"
} else when linux {
    import "platform_linux.bsn"
}

```

For inline assembly you may do something like this unless the compiler is aware
of the architecture.

```odin
when x86_64 {
    import "platform_x86_64.bsn"

    // or write the platform function directly here

} else when aarch64 {
    import "platform_aarch64.bsn"
}
```


Another reason for conditional compilation is profiling code.


```odin

fn work() {

    when ENABLE_PROFILING {
        start_profiling()
        defer end_profiling(handle)
    }

}

```

What is very important with conditional compilation is that the left out code is
still parsed and type checked. Otherwise it can stagnate and cause unforseen errors
if you didn't compile for windows (or linux) in a while.


If we put imports in conditional compilation then it's a little hard to keep parse and type check the other part.
You parse the import but nothing references any functions in it. What if it calls some compile time stuff and use
windows specific functions on linux?

```

// platform_windows.bsh



platform_linux


// main.bsn

when windows {
    import "platform_windows.bsn"
} else when linux {
    import "platform_linux.bsn"
}

```


# Profiling

Mark all functions you want to add profiling to. Perhaps you can pattern match all functions that
are compiled. Wildcard "*" or wildcard on files in certain directories or with certain file names.

```odin

@profiled(stage=ast)
fn work() {
    // do work
}

@profiled_early(stage=text)
fn work() {
    // do work
}

// profiling.bsn

import "basin" as basin

fn profiled(func: ^basin.ASTFunction) {

    // construct node tree
    node_tree = #code {
        frame: ProfileFrame
        frame.start_ticks := cpu_ticks()
        defer {
            frame.end_ticks := cpu_ticks()

        }
    }
    
    // add node to beginning of function
    basin_pre_append_node(func, node_tree)

    when ENABLED_LOGGING {
        basin_post_append_text(func, f'printf("Exit {func.name}")')
    }
}

fn profiled_early(func_text: string) -> string {

    // Get below as text
    extra_text = "
        start_ticks := cpu_ticks()
        defer {
            end_ticks := cpu_ticks()
            total_ticks := end_ticks - start_ticks
        }
    "
    
    return extra_text + func_text
}

```

What is complicated with the above is the order at which we should parse and compile things.
The normal profiled function works on an already compiled function where we prepend nodes and append text. It has not been type checked yet.

The second profiled function runs on a very early function body that hasn't been parsed yet.
Perhaps the parsing of that body has been delayed because an annotation has been added to it.


Above you write meta and normal code in the same program.
Maybe the meta code can be C code? Maybe it is a shared library you compile and attach to the compiler?

I would prefer if you wrote it as meta code and it was compiled along the program. Otherwise you must mangage build system for the shared library and the compilation of C code if you choose that route. Also, C code doesn't have `#code` to reference a node tree as a value.




You may want to add prolog and epilog to all functions (or all functions in certain files and modules).
Stack trace can be implemented this way (for performance you would use debug information to avoid additional code in all functions).


# Asserts

```c

#import "Logger"
#import "StackTrace"

#if DISABLE_ASSERTS
    #macro Assert(EXPR) #endmacro
#else
    #macro Assert(EXPR) CheckAssert(EXPR, #line, #file, #quoted EXPR)
#endif

fn CheckAssert(condition: bool, line: i32, file: char[], expr_string: char[]) {
    // TODO: NOT THREAD SAFE, we need a boolean per thread. thread local storage?
    global disable_asserts: bool;
    loc: i32;
    if condition
        return;
        
    if disable_asserts
        return;
        
    disable_asserts = true
    defer disable_asserts = false
    
    // PrintStackTrace()
    
    prints("\n");
    
    prints("[ASSERT] ")
    prints(expr_string)
    prints(" - ")
    prints(file)
    prints(":")
    std_print(line)
    prints("\n");
    
    *(cast<i32*>null) = 0
}

```



# Linker symbol names for functions

We have two libraries written in basin and link them into an executable.

Both of them use the basin standard library. If those functions are public (not C static functions) then they will collide.

So you need to choose whether to use functions from the standard library as private static functions, as static library where functions are extern symbols, not resolved, or as dynamic library where they are external to. For the static/dynamic library you must link with them.

If we do private static functions then each object file would have the same functions standard library functions which is quite inefficient.

Imagine doing
```makefile
SRCFILES := $(wildcard src/%.bsn )
%.o: %.bsn
    basin -o $@ $^
```

How do we support these use cases? In the standard library we don't want to bloat the source code with optional @external everywhere.

Your library may be used by multiple libraries and you have the same problem? If you have two libraries that include stb_image and both declare public functions? That's just bad library design. How do we not do bad standard library design?


Maybe the "import" determines how symbols should behave.
```odin

@private
import "util.bsn"

util_print()

@external_sta
import "util.bsn"

util_print()

@external_dyn
import "util.bsn"

util_print()

```

It doesn't work because how someone imports the library determines how it should be compiled.

If the same program has two basin files where one imports with @private and the other with @external_sta then they would collide.
We need to decide on a global level how files should be compiled. By default everything is private.

@external and @export can be used to say these should be public symbols.

If we think my OS how do i want linking, toolchain, compiler, symbols to work. I still want concept of local and global symbols (private, public i am mixing the terminology a lot here). You can define the same function in different object files without them colliding (print helper function for example). Also you don't want to pollute the global symbol space with all functions in the program. Object files especially from different libraries
may name internal functions the same. SO, there is a need for internal/local/private functions.

For static and dynamic libraries i would like there to be no difference. You compile and use them the same way. Some libraries you want to link with executable right away. Some libraries are plugins you load at runtime. Some libraries can be hotreloaded and updated while the program is running.

The reason static and dynamic libraries are separater is because relocations and function calls on x86 (and all other architectures) are different.




For dynamic libraries what happens if you unload and reload them. All global variables are deleted.
You might be doing some hotreloading and you wanted to keep the global variables.
So the standard library should not use any global variables?
That seems a little difficult. We could use thread local storage and heap allocate any memory standard library
uses internally (linear heap allocation or something efficient). Now our target platform has to support thread local
storage.



# Declarations and configurations




