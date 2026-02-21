#pragma once

#include "basin/types.h"
#include "basin/frontend/lexer.h" // SourceLocation
#include "util/array.h"

#define DEBUG_BUILD

typedef enum {
    EXPR_BLOCK,
    EXPR_FOR,
    EXPR_WHILE,
    EXPR_IF,
    EXPR_SWITCH,
    EXPR_CALL,
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
    string name;
    ASTExpression* expr;
} ASTExpression_Initializer_Element;

DEF_ARRAY(ASTExpression_Initializer_Element);

typedef struct {
    NODE_BASE

    Array_ASTExpression_Initializer_Element elements;
} ASTExpression_Initializer;


typedef struct {
    NODE_BASE

    string name;

} ASTExpression_Identifier;

typedef struct {
    SourceLocation location; // points at name if exists, otherwise expr
    string name; // named argument
    ASTExpression* expr;
} ASTExpression_Call_Argument;

DEF_ARRAY(ASTExpression_Call_Argument);

typedef struct {
    // For now just expression, we may add more like named polymorphic arguments
    // call<Key=int,Val=string>()
    ASTExpression* expr; // type name, number or whatever
} ASTExpression_Call_PolyArgument;

DEF_ARRAY(ASTExpression_Call_PolyArgument);

typedef struct {
    NODE_BASE

    ASTExpression* expr; // identifier or expr that evaluates to function pointer
    Array_ASTExpression_Call_PolyArgument polymorphic_args;
    Array_ASTExpression_Call_Argument arguments;
} ASTExpression_Call;

typedef struct ASTExpression_Block ASTExpression_Block;

// Memory owner
typedef struct AST {
    TokenStream* stream;
    ASTExpression_Block* global_block;
} AST;

typedef struct ASTAnnotation {
    string name;
    string content;
} ASTAnnotation;


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

    ASTExpression* ref;
    ASTExpression* value;
} ASTExpression_Assign;

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
    Array_ASTExpressionP conditions; // empty for default case
    ASTExpression* body;
} ASTExpression_Switch_Case;

DEF_ARRAY(ASTExpression_Switch_Case)

typedef struct {
    NODE_BASE

    ASTExpression* selector;
    Array_ASTExpression_Switch_Case cases; // only last case may be the default case, default case is optional
} ASTExpression_Switch;

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
    ASTExpression* default_value;
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
    ASTType type_name;
    ASTExpression* value;
} ASTGlobal, ASTConstant;

typedef struct {
    SourceLocation location;
    string name;
    ASTType type_name;
} ASTVariable;

typedef struct {
    SourceLocation location;
    string name;
    Array_ASTStruct_Field fields;
} ASTStruct;

typedef struct {
    SourceLocation location;
    string name;
    ASTExpression* default_value;
} ASTEnum_Member;

DEF_ARRAY(ASTEnum_Member);

typedef struct {
    SourceLocation location;
    string name;
    ASTType type_name; // base type, i8,u32...
    Array_ASTEnum_Member members;
    bool share;
} ASTEnum;


typedef struct {
    SourceLocation location;
    string         name; // may be empty otherwise name comes from 'import "util" as name'
    bool           shared;
    Import*        import;
} ASTImport;


typedef struct {
    SourceLocation location;
    string name;         // as name
    string library_name;
} ASTLibrary;

typedef ASTFunction* ASTFunctionP;
typedef ASTStruct*   ASTStructP;
typedef ASTEnum*     ASTEnumP;
typedef ASTVariable* ASTVariableP;
typedef ASTConstant* ASTConstantP;
typedef ASTGlobal*   ASTGlobalP;
// typedef ASTImport*   ASTImportP;
// typedef ASTLibrary*  ASTLibraryP;

DEF_ARRAY(ASTFunctionP)
DEF_ARRAY(ASTStructP)
DEF_ARRAY(ASTEnumP)
DEF_ARRAY(ASTVariableP)
DEF_ARRAY(ASTConstantP)
DEF_ARRAY(ASTGlobalP)
DEF_ARRAY(ASTImport)
DEF_ARRAY(ASTLibrary)


struct ASTExpression_Block {
    NODE_BASE

    ASTExpression_Block* parent; // NULL means file level block
    Array_ASTImport      imports;
    Array_ASTLibrary     libraries;

    // Unordered list of functions, structs, enums, globals, constants
    // Consider not making these pointers?
    // Consider using small and large blocks.
    // Large blocks for the global import scope
    // small for all else.
    // large blocks use bucket arrays?
    Array_ASTVariableP   variables;
    Array_ASTGlobalP     globals;
    Array_ASTConstantP   constants;
    Array_ASTFunctionP   functions;
    Array_ASTEnumP       enums;
    Array_ASTStructP     structs;

    // list of expressions
    Array_ASTExpressionP expressions;
};

typedef enum {
    FOUND_NONE = 0,
    FOUND_VARIABLE,
    FOUND_GLOBAL,
    FOUND_CONSTANT,
    FOUND_FUNCTION,
    FOUND_IMPORT,
    FOUND_LIBRARY,
    FOUND_STRUCT,
    FOUND_ENUM,
    FOUND_ENUM_MEMBER,
} FoundKind;
typedef struct {
    FoundKind        kind;
    ASTVariable*     f_variable;
    ASTGlobal*       f_global;
    ASTConstant*     f_constant;
    ASTFunction*     f_function;
    ASTImport*       f_import;
    ASTLibrary*      f_library;
    ASTStruct*       f_struct;
    ASTEnum*         f_enum;
    ASTEnum_Member*  f_enum_member;

    ASTExpression_Block* block;
} FindResult;
bool find_identifier(cstring name, AST* ast, ASTExpression_Block* block, FindResult* result);

// returns index of parameter
// -1 if not found
int find_function_parameter(cstring name, ASTFunction* func);


void print_ast(AST* ast);
void print_expression(ASTExpression* expr, int depth);
void print_import(ASTImport* imp, int depth);
void print_function(ASTFunction* func, int depth);
void print_struct(ASTStruct* struc, int depth);
void print_enum(ASTEnum* enu, int depth);
void print_global(ASTGlobal* object, int depth);
void print_constant(ASTConstant* object, int depth);
void print_variable(ASTVariable* object, int depth);
