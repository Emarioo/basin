Tests to write


# Errors
- Global not allowed inside comptime expression
- Comptime expr. not allowed in comptime expression
- Import not allowed in comptime expression.

# Working
- compiler_import(\"windows\") works in comptime expression.
- If statements for operating system and compiler_import specific OS module.
- comptime emit
- comptime eval
- comptime normal


# Object File
- `dumpbin /all` and `objdump -x` on COFF file, search for errors in the output.
- `objdump -x` on ELF file, search for errors in the output.
