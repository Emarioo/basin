Basin compiler
(based on [BetterThanBatch](https://github.com/Emarioo/BetterThanBatch))

# Current state

```bash
> ./build.py && basin dev.bsn

[-1] Add task TASK_LEX_AND_PARSE
[0] Started
[0] waiting
[0] pick TASK_LEX_AND_PARSE (0 left)
ANOT external 'libc'
root: BLOCK
  FUNCTION write
  FUNCTION main
    BLOCK
      VARIABLE text : char[]
      ASSIGN
        IDENTIFIER text
        LITERAL "hello\n"
      CALL IDENTIFIER write
        arg0: LITERAL 1
        arg1: MEMBER ptr
          IDENTIFIER text
        arg2: MEMBER len
          IDENTIFIER text
      RETURN
        LITERAL 5
[0] Add task TASK_GEN_IR
[0] waiting
[0] pick TASK_GEN_IR (0 left)
Gen Func write
 skip no body write
Gen Func main
  var_addr r1, [.stack + 4]
  imm r2, 7
  var_addr r3, [.rodata + 0]
  store [r1 + 0], r3
  store [r1 + 8], r2
  imm r1, 1
  var_addr r2, [.stack + 16]
  load r2, [r2 + 6]
  var_addr r3, [.stack + 16]
  load r3, [r3 + 6]
  call write, r1, r2, r3 -> r4
  imm r1, 5
  ret r1
Gen ir success
[0] Add task TASK_GEN_MACHINE
[0] waiting
[0] pick TASK_GEN_MACHINE (0 left)
  000b: lea 2, [rip+?] (requires relocation)
hexdump 0x1fc2c85d890 + 0x2f:
  1fc2c85d890: 8d 44 24 04 b9 07 00 00 00 8d 15 00
  1fc2c85d89c: 00 00 00 89 10 89 48 08 bb 01 00 00
  1fc2c85d8a8: 00 8d 74 24 10 8b 7f 06 44 8d 44 24
  1fc2c85d8b4: 10 45 8b 49 06 41 ba 05 00 00 00
Gen machine success
[0] waiting
[0] Stopped
Threads finished
Success (no object file is generated, IR and x86 machine code is flawed)
```


# Building

(Same for Windows and Linux)

**Dependencies**
- Python 3.11 (3.9 probably works too)
- GCC

**Compiling**
```bash
git clone https://github.com/Emarioo/basin
cd basin
python3 build.py
```
**Usage**
```bash
# Linux
source env.sh
which basin
/mnt/d/dev/basin/releases/basin-0.0.1-linux-x86_64/bin/basin

# Windows
env.bat
where basin
D:\dev\basin\releases\basin-0.0.1-windows-x86_64\bin\basin.exe

# Dev example (latest feature being worked on)
basin dev.bsn
```

# folders
```bash
docs         - Documentation, guides, language specification
include      - Public API headers to use basin as library
import       - Basin standard library
src/basin    - source code of compiler, completely platform/operating system independent
src/platform - platform API, basic data structures, memory allocators. used by src/basin
tracy        - Tracy for profiling the compiler
tests        - Basin tests

# These are ignored by git
releases     - Built basin packages and releases
int          - Intermediate object files
```