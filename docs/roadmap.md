
# Roadmap for Basin compiler and language

For 1.0.0 of Basin we need the following:

|Status|Thing|Description|
|-|-|-|
|1%|Complete Basin language|After 1.0.0 backwards compatible changes will be made (to language, compiler, and standard library)|
|0%|Mostly complete standard library| Modules for OS platform, atomics, memory, array structures, time.|
|1%|Complete x86_64 backend|Correct calling conventions, code generation, struct returns, stack alignment, section alignment with respect to aligned SIMD instructions, ABI compatibility, Linux/Windows support. Optimizations can be improved upon in later versions.|
|0%|Working ARM backend|The goal is to show that the compiler isn't limited to x86. Code may not be optimized but most tests should pass to consider it working.|
|0%|Debugging with GDB|We start with DWARF support. May add PDB which is Windows specific later.|
|0%|1,000,000 lines per second|One of the goals of the compiler is being fast. If it isn't in version 1.0.0 then we have failed.|

# Basin Language

Syntax, type system, import logic, compile time execution finalized

Types
Functions
Structs
Imports
C header imports
Runtime Type Information
Inline Assembly

# Standard library

The following modules should exist:
- Core
    - Memory
    - String
    - Unicode, UTF-8
    - DynamicArray
    - HashMap
    - Math
    - Random
    - Runtime Type Information
- Platform
    - FileSystem (CWD, open, read, write, iterate directory, fstat)
    - Thread
    - Process
    - Network
    - Atomic
    - Time
    - Date
- Media
    - Graphic
    - Audio
    - Image (stb_image)
- Utilities
    - Command Line Parsing

Some details we shouldn't forget (very common and useful things):
- Path functions: dirname, abspath, split extension.
- Date functions: UTC, local time?