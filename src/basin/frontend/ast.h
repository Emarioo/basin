#pragma once

#include "basin/types.h"
#include "basin/frontend/lexer.h" // SourceLocation
#include "platform/array.h"

#define DEBUG_BUILD

typedef enum {
    EXPR_BLOCK,
    EXPR_FOR,
    EXPR_WHILE,
    EXPR_IF,
    EXPR_CALL, // also arithmetic, logical, comparison operation
    EXPR_RETURN,
    EXPR_YIELD,
    EXPR_CONTINUE,
    EXPR_BREAK,
    EXPR_ASSEMBLY,

    EXPR_ASSIGN,
    EXPR_MEMBER,
    EXPR_CAST,

    EXPR_IDENTIFIER,
    EXPR_INITIALIZER,
    EXPR_LITERAL,

    EXPR_UNARY,
    EXPR_BINARY,
} _ExpressionKind;

typedef enum {
    EXPR_OP_ADD,
    EXPR_OP_SUB,
    EXPR_OP_MUL,
    EXPR_OP_DIV,
    EXPR_OP_MODULO,

    EXPR_OP_LESS,
    EXPR_OP_GREATER,
    EXPR_OP_LESS_EQUAL,
    EXPR_OP_GREATER_EQUAL,
    EXPR_OP_EQUAL,
    EXPR_OP_NOT_EQUAL,

    EXPR_OP_BITWISE_OR,
    EXPR_OP_BITWISE_AND,
    EXPR_OP_BITWISE_XOR,
    EXPR_OP_BITWISE_LSHIFT,
    EXPR_OP_BITWISE_RSHIFT,
    EXPR_OP_BITWISE_NEGATE,

    EXPR_OP_LOGICAL_NOT,
    EXPR_OP_LOGICAL_AND,
    EXPR_OP_LOGICAL_OR,
    
    EXPR_OP_ADDRESS_OF,
    EXPR_OP_DEREF,
} _OperatorKind;

typedef enum {
    EXPR_LITERAL_INTEGER,
    EXPR_LITERAL_FLOAT,
    EXPR_LITERAL_STRING,
} _LiteralKind;

typedef u8 ExpressionKind;
typedef u8 OperatorKind;
typedef u8 LiteralKind;

typedef string ASTType;

#ifdef DEBUG_BUILD
#define NODE_BASE   SourceLocation location;  \
                    _ExpressionKind kind;
#else
#define NODE_BASE   SourceLocation location;  \
                    ExpressionKind kind;
#endif

typedef struct {
    NODE_BASE
} ASTExpression;

typedef struct {
    NODE_BASE

    ASTExpression* expr;
    string name;
} ASTExpression_Member;

typedef struct {
    NODE_BASE

    ASTExpression* expr;
    ASTType type_name;
} ASTExpression_Cast;

typedef struct {
    NODE_BASE
    #ifdef DEBUG_BUILD
        _OperatorKind op_kind;
    #else
        OperatorKind op_kind;
    #endif

    ASTExpression* expr;
} ASTExpression_Unary;

typedef struct {
    NODE_BASE
    #ifdef DEBUG_BUILD
        _OperatorKind op_kind;
    #else
        OperatorKind op_kind;
    #endif

    ASTExpression* left;
    ASTExpression* right;
} ASTExpression_Binary;



typedef struct {
    NODE_BASE
    #ifdef DEBUG_BUILD
        _LiteralKind literal_kind;
    #else
        LiteralKind literal_kind;
    #endif

    union {
        int64_t int_value;
        double  float_value;
        string  string_value;
    };
} ASTExpression_Literal;

typedef struct {
    NODE_BASE

    string name;

} ASTExpression_Identifier;

typedef struct {
    string name;
    ASTExpression* expr;
} ASTExpression_Initializer_Element;

DEF_ARRAY(ASTExpression_Initializer_Element);

typedef struct {
    NODE_BASE

    Array_ASTExpression_Initializer_Element elements;
} ASTExpression_Initializer;


// Memory owner
typedef struct {
    ASTExpression* global_block;
} AST;




typedef ASTExpression* ASTExpressionP;
DEF_ARRAY(ASTExpressionP)


typedef ASTExpression ASTExpression_Continue;
typedef ASTExpression ASTExpression_Break;


typedef struct {
    NODE_BASE

    string index_name;
    string item_name;

    ASTExpression* condition_expr;
    ASTExpression* body_expr;
} ASTExpression_For;


typedef struct {
    NODE_BASE

    ASTExpression* condition_expr;
    ASTExpression* body_expr;
} ASTExpression_While;

typedef struct {
    NODE_BASE

    ASTExpression* condition_expr;
    ASTExpression* body_expr;
    ASTExpression* else_expr;
} ASTExpression_If;

typedef struct {
    NODE_BASE

    Array_ASTExpressionP exprs;
} ASTExpression_Return, ASTExpression_Yield;


typedef ASTExpression* ASTExpressionP;


typedef struct {
    string name;
} TypeInfo;

typedef TypeInfo* TypeInfoP;

DEF_ARRAY(TypeInfoP)

typedef struct {
    Array_TypeInfoP arguments;
    Array_TypeInfoP return_types;
} FunctionSignature;

typedef struct {
    SourceLocation location;
    string name;
    ASTType type_name;
    ASTExpression* default_value;
} ASTFunction_Parameter;

DEF_ARRAY(ASTFunction_Parameter);

typedef struct {
    SourceLocation location;
    string name;
    ASTType type_name;
    // ASTExpression* type_expr;
} ASTStruct_Field;

DEF_ARRAY(ASTStruct_Field);

typedef struct {
    SourceLocation location;
    string name;
    FunctionSignature signature;
    Array_ASTFunction_Parameter parameters;
    Array_ASTFunction_Parameter return_values;
    ASTExpression* body;
} ASTFunction;

typedef struct {
    SourceLocation location;
    string name;
    Array_ASTStruct_Field fields;
} ASTStruct;

typedef ASTFunction* ASTFunctionP;
typedef ASTStruct*   ASTStructP;

DEF_ARRAY(ASTFunctionP)
DEF_ARRAY(ASTStructP)


typedef struct {
    NODE_BASE

    // Unordered list of functions, structs, enums, globals, constants
    // Array_ASTExpressionP globals;
    // Array_ASTExpressionP constants;
    Array_ASTFunctionP   functions;
    // Array_ASTExpressionP enums;
    Array_ASTStructP structs;

    // list of expressions
    Array_ASTExpressionP expressions;
} ASTExpression_Block;


void print_ast(AST* ast);
void print_expression(ASTExpression* expr, int depth);
void print_function(ASTFunction* func, int depth);
void print_struct(ASTStruct* struc, int depth);
