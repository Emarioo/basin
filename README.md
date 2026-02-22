Basin compiler
(based on [BetterThanBatch](https://github.com/Emarioo/BetterThanBatch))

# Current state

```bash
./build.py && basin dev.bsn

Basin compiled, bin/basin
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
  call main, r1, r2, r3 -> r4
  imm r5, 5
  ret r5
Gen ir success
[0] waiting
[0] Stopped
Threads finished
Success (x86 codegen not implemented yet)
```


# Building

## Dependencies
- Python 3.11 (3.9 might work to)
- GCC

```bash
python3 build.py
```

# folders
```
docs         - Documentation, guides, language specification
include      - Public API headers to use basin as library
modules      - Basin standard library
src/basin    - source code of compiler, completely platform/operating system independent
src/platform - platform API, basic data structures, memory allocators. used by src/basin
tests        - Basin tests
bin          - Built executables and binaries
bin/int      - Intermediate object files
```