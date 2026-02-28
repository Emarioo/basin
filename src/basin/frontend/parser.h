/*
    Overview about the parser's design:

        Every token we parse exists inside an expression.
        Anything at the global level is parse within an expression.
        Arithmetic, standalone functions and other values we parse are part of an expression
        for, switch, if-statements exist within expressions too.

        The different is function, struct, enum, global variable declarations and imports.

        While they are located within an expression they are not part of the execution flow. They declare types, code, and content from other files.

        Function calls, for, switch, variable assignments are part of the execution flow and stored as an execution sequence inside expressions.
        The declarative objects are unordered and just exist within the expressions as something you may use.

        Note that all language features (structs, functions, for, while) are located inside expressions but they are not all one big union inside ASTExpression.
*/

#pragma once

#include "basin/types.h"

typedef struct Driver Driver;
typedef struct AST AST;
typedef struct TokenStream TokenStream;

Result parse_stream(Compilation* compilation, TokenStream* stream, AST** out_ast);
void print_ast(AST* ast);
