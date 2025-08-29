
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



# Multithreading

Lexing a file is one operation. It takes input and gives output. It does not modify some global state. (it may add some new file ids if you do #paste)

# Error handling
If lexing or parsing fails we immediately stop and mark that import as failed. We don't quit the compiler process right away.
We can lex/parse multiple fails and have them all fail. That's okay. We just don't try to catch more erros by continuing parsing.

If parser lexer fails, WE DO NOT continue to the type checker phase. Because an import with a function we need may have failed and we don't have access to it so identifier lookup will fail creating tons of errors.