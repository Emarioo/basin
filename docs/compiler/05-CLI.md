Command Line Interface for Basin Compiler

Features we provide:
- Compile Basin program to
    - Object file
    - Executable
    - Static Library
    - Dynamic Library
- Run Basin program
- Extra flags that affect compilation
    - Optimizations
    - Debug Information
    - Target architecture and OS
    - Silent
- Dumping
    - AST
    - IR
    - Driver task scheduling
    - Import resolution


```bash

# run
basin main.bsn
basin main.bsn -run

# object file
basin main.bsn -o main.o
basin main.bsn -o main.obj

# static library
basin main.bsn -o libmain.a
basin main.bsn -o main.lib

# shared library
basin main.bsn -o libmain.so.5.2
basin main.bsn -o main.dll

# executable
basin main.bsn -o main
basin main.bsn -o main.exe

# debug info
basin main.bsn -o main -debug 
basin main.bsn -o main -debug=none
basin main.bsn -o main -debug=dwarf
basin main.bsn -o main -debug=pdb

# optimization
basin main.bsn -o main -optimization=none
basin main.bsn -o main -O=speed
basin main.bsn -o main -O=size

# target
basin main.bsn -o main.o -target x86_64-windows
basin main.bsn -o main.o -target x86_64-linux
basin main.bsn -o main.o -target aarch64-linux
basin main.bsn -o main.o -target arm-eabi
basin main.bsn -o main.o -target i386-sysv
basin main.bsn -o main.o -target x86_64-sysv
basin main.bsn -o main.o -target x86_64-msx64

# explicit target
basin main.bsn -o main.o -mos windows \
                         -mformat coff \
                         -march x86_64

basin main.bsn -o main.o -mos windows \
                         -mabi msx64 \
                         -mformat coff \
                         -march x86_64

basin main.bsn -o main.o -mabi eabi \
                         -mformat elf \
                         -march armv7

# CPU extensions
basin main.bsn -o main.o -mfeature=avx-512,bmi2

# Dumping
basin main.bsn -o main.o -dump-ast
basin main.bsn -o main.o -dump-ir
basin main.bsn -o main.o -dump-driver

# Silence success and compile time info
basin main.bsn -o main.o -silent

```