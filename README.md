Basin compiler
(based on [BetterThanBatch](https://github.com/Emarioo/BetterThanBatch))


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