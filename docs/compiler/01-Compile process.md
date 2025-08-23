
Steps

1. Lexer: transform text into tokens
2. Parser: transform tokens into syntax tree
3. Type checking, constant evaluation, macro expansion, metaprogramming
4. Bytecode gen.: Create bytecode from AST
5. Machine code gen.: Create x86/ARM from bytecode.


# Lexer
- Ignore comments.
- Parse escape characters in strings.
- Seperate words into keywords and identifiers.

We may potentially support local macro character expansion in the lexer.

The lexer takes a file or text as input and produces a sequence of tokens.

# Parser