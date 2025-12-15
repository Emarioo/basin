/*
    The parsing and matching pretty much always uses TokenExt since it encompasses
    Token and TokenExt.
*/

#include "basin/frontend/parser.h"

#include "basin/core/driver.h"
#include "basin/frontend/ast.h"

#include "platform/memory.h"

#include <setjmp.h>
#include <stdarg.h>

#define match(KIND) _match(context, KIND);
// 0 current token
#define peek(N) _peek(context, N)
#define advance() _advance(context)

typedef enum {
    COMPTIME_NONE,
    COMPTIME_NORMAL,
    COMPTIME_EVAL,
    COMPTIME_EMIT,
} ComptimeKind;

typedef struct {
    Driver* driver;
    TokenStream* stream;
    int head; // token head
    
    ASTExpression_Block* previous_block;

    ComptimeKind comptime_kind;
    
    jmp_buf jump_state;
    TokenExt bad_token; // token causing bad syntax
    CLocation c_location; // token causing bad syntax
    char error_message[256];
} ParserContext;


#define CREATE_EXPR(V, T, KIND, TOK)      \
    T* V = HEAP_ALLOC_OBJECT(T);          \
    V->kind = KIND;                       \
    V->location = location_from_token(TOK);


#define SET_EXPR(V, T, KIND, TOK)         \
    V = HEAP_ALLOC_OBJECT(T);          \
    V->kind = KIND;                       \
    V->location = location_from_token(TOK);


ASTType parse_type(ParserContext* context);
ASTExpression* parse_expression(ParserContext* context);
ASTExpression* parse_block_expression(ParserContext* context, bool at_file_scope);
ASTFunction* parse_function(ParserContext* context);
ASTStruct* parse_struct(ParserContext* context);

// ASTExpression* create_expression(ParserContext* context, ExpressionKind kind) {
//     // @TODO Use linear allocator?
//     ASTExpression* expr = HEAP_ALLOC_OBJECT(ASTExpression);
//     return expr;
// }

Result parse_stream(Driver* driver, TokenStream* stream, AST** out_ast) {
    // We implement this recursively because it's easier to debug issues
    Result result;
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    AST* ast = HEAP_ALLOC_OBJECT(AST);
    ast->stream = stream;

    ParserContext context = {0};
    context.stream = stream;
    context.driver = driver;
    context.head = 0;

    int res = setjmp(context.jump_state);

    if (res == 0) {

        ast->global_block = parse_block_expression(&context, true);

        print_ast(ast);

        *out_ast = ast;
    } else {
        int line, column;
        string code;
        bool yes = compute_source_info(stream, location_from_token(&context.bad_token), &line, &column, &code);

        char buffer[1024];
        int len = 0;
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[37m%s:%d\033[0m\n", context.c_location.path, context.c_location.line);
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[31m%s:%d:%d:\033[0m %s\n%s", stream->import->path.ptr, line, column, context.error_message, code.ptr);
        result.kind = FAILURE;
        result.message = string_clone_cptr(buffer);
    }

    return result;
}

#define check_error_ext(T, ...) ((T)->kind == T_END_OF_FILE ? parse_error((const TokenExt*)(void*)(T), __VA_ARGS__) : 0)

#define parse_error(TOK, FMT, ...) ( context->c_location.path = __FILE__, context->c_location.line = __LINE__, _parse_error(context, TOK, FMT __VA_OPT__(,)  __VA_ARGS__) )

void _parse_error(ParserContext* context, const TokenExt* tok, char* fmt, ...) {
    context->bad_token = *(const TokenExt*)tok;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(context->error_message, sizeof(context->error_message), fmt, ap);
    va_end(ap);

    longjmp(context->jump_state, 1);
}

static const TokenExt* _peek(ParserContext* context, int n) {
    int head = context->head;
    const TokenExt* tok = NULL;
    while((n+1)) {
        n--;
        if (head >= context->stream->tokens_len)
            return &EOF_TOKEN_EXT;
        tok = (const TokenExt*)&context->stream->tokens[head];
        if (IS_EXT_TOKEN(tok->kind))
            head += TOKEN_PER_EXT_TOKEN;
        else
            head+=1;
    }
    return tok;
}

static const TokenExt* _match(ParserContext* context, TokenKind kind) {
    const TokenExt* tok = _peek(context, 0);
    if (tok->kind == kind) {
        context->head += 1 + (TOKEN_PER_EXT_TOKEN - 1) * IS_EXT_TOKEN(kind);
        return tok;
    } else {
        parse_error((const TokenExt*)&context->stream->tokens[context->head], "Unexpected token, wanted %s", name_from_token(kind));
        return &EOF_TOKEN_EXT;
    }
}


static const TokenExt* _advance(ParserContext* context) {
    if (context->head >= context->stream->tokens_len)
        return &EOF_TOKEN_EXT;
    const TokenExt* tok;
    tok = (const TokenExt*)&context->stream->tokens[context->head];
    if (IS_EXT_TOKEN(tok->kind))
        context->head += TOKEN_PER_EXT_TOKEN;
    else
        context->head += 1;
    return tok;
}

void enter_comptime_mode(ParserContext* context, ComptimeKind kind) {
    ASSERT(context->comptime_kind == COMPTIME_NONE);
    context->comptime_kind = kind;
}
void exit_comptime_mode(ParserContext* context) {
    ASSERT(context->comptime_kind != COMPTIME_NONE);
    context->comptime_kind = COMPTIME_NONE;
}

#define IS_COMPTIME() (context->comptime_kind != COMPTIME_NONE)

ASTExpression* parse_block_expression(ParserContext* context, bool at_file_scope) {

    const TokenExt* block_tok = peek(0);
    if (!at_file_scope) {
        match('{');
    }
    
    CREATE_EXPR(block_expr, ASTExpression_Block, EXPR_BLOCK, block_tok);
    block_expr->parent = context->previous_block;
    context->previous_block = block_expr;

    while (true) {
        const TokenExt* tok  = peek(0);
        const TokenExt* tok1 = peek(1);

        if (IS_EOF(tok) && at_file_scope) {
            break;
        }

        // TODO: Parse annotations
        if(tok->kind == T_IMPORT) {
            advance();

            if(IS_COMPTIME()) {
                parse_error(tok, "Import is not allowed in compile time expression. Use 'compiler_import(\"windows\")' from 'import \"compiler\"' instead.");
            }

            const TokenExt* tok = match(T_LITERAL_STRING);

            cstring path = DATA_FROM_STRING(tok);
        
            // @TODO Check if import already exists.
            Task task = {};
            task.kind = TASK_TOKENIZE;
            task.tokenize.import = driver_create_import_id(context->driver, path);

            driver_add_task(context->driver, &task);

            // @TODO Submit ASTImport to tokenize task so it can be updated?
            ASTImport new_import = {};
            new_import.location = location_from_token(tok);
            new_import.name = string_clone_cstr(path);
            new_import.shared = false; // set from annontation @shared
            array_push(&block_expr->imports, &new_import);

            // TODO: Parse as

            // @TODO Add import to scope tree

        } else if (tok->kind == T_GLOBAL) {
            advance();
            
            const TokenExt* ident_tok = match(T_IDENTIFIER);
            match(':');
            
            ASTGlobal* data_object = HEAP_ALLOC_OBJECT(ASTGlobal);
            data_object->location = location_from_token(ident_tok);
            
            cstring name = DATA_FROM_TOKEN(ident_tok);
            data_object->name = string_clone_cstr(name);
            
            
            data_object->type_name = parse_type(context);
            
            const TokenExt* equal_tok = peek(0);
            if (equal_tok->kind == '=') {
                data_object->value = parse_expression(context);
            }

            array_push(&block_expr->globals, &data_object);
        } else if (tok->kind == T_CONST) {
            advance();
            
            const TokenExt* ident_tok = match(T_IDENTIFIER);
            match(':');
            
            ASTConstant* data_object = HEAP_ALLOC_OBJECT(ASTConstant);
            data_object->location = location_from_token(ident_tok);
            
            cstring name = DATA_FROM_TOKEN(ident_tok);
            data_object->name = string_clone_cstr(name);
            
            data_object->type_name = parse_type(context);
            
            const TokenExt* equal_tok = peek(0);
            if (equal_tok->kind == '=') {
                data_object->value = parse_expression(context);
            }
            array_push(&block_expr->constants, &data_object);
        } else if (tok->kind == T_IDENTIFIER && tok->kind == ':') {
            advance();
            advance();
            
            ASTVariable* data_object = HEAP_ALLOC_OBJECT(ASTVariable);
            data_object->location = location_from_token(tok);
            
            cstring name = DATA_FROM_TOKEN(tok);
            data_object->name = string_clone_cstr(name);

            data_object->type_name = parse_type(context);
            
            const TokenExt* equal_tok = peek(0);
            if (equal_tok->kind == '=') {

                ASTExpression* rvalue = parse_expression(context);

                CREATE_EXPR(expr_lval, ASTExpression_Identifier, EXPR_IDENTIFIER, tok);
                expr_lval->name = string_clone_cstr(name);

                CREATE_EXPR(expr_assign, ASTExpression_Assign, EXPR_ASSIGN, equal_tok);
                expr_assign->ref = (ASTExpression*) expr_lval;
                expr_assign->value = rvalue;

                array_push(&block_expr->expressions, (ASTExpression**)&expr_assign);
            }

            array_push(&block_expr->variables, &data_object);

        } else if (tok->kind == T_STRUCT) {
            
            ASTStruct* struc = parse_struct(context);
            array_push(&block_expr->structs, &struc);

        } else if (tok->kind == T_FN) {
            
            ASTFunction* function = parse_function(context);
            array_push(&block_expr->functions, &function);

        } else if (tok->kind == '}') {
            if (!at_file_scope) {
                break;
            }
            parse_error(tok, "Unexpected closing brace. (at file scope)");
        } else {
            ASTExpression* expr = parse_expression(context);

            const TokenExt* equal_tok = peek(0);
            if (equal_tok->kind == '=') {
                ASTExpression* rvalue = parse_expression(context);

                CREATE_EXPR(expr_assign, ASTExpression_Assign, EXPR_ASSIGN, equal_tok);
                expr_assign->ref = expr;
                expr_assign->value = rvalue;

                array_push(&block_expr->expressions, (ASTExpression**)&expr_assign);
            } else {
                array_push(&block_expr->expressions, &expr);
            }
        }
    }

    if (!at_file_scope) {
        match('}');
    }

    context->previous_block = block_expr->parent;

    return (ASTExpression*)block_expr;
}

int op_precedence(int op_kind) {
    // @TODO Table?
    switch(op_kind) {
        case EXPR_OP_BITWISE_LSHIFT:
        case EXPR_OP_BITWISE_RSHIFT:
            return 9;
        case EXPR_OP_BITWISE_AND:
            return 7;
        case EXPR_OP_BITWISE_XOR:
            return 8;
        case EXPR_OP_BITWISE_OR:
            return 6;
        case EXPR_OP_MUL:
        case EXPR_OP_DIV:
        case EXPR_OP_MODULO:
            return 5;
        case EXPR_OP_ADD:
        case EXPR_OP_SUB:
            return 4;
        case EXPR_OP_LESS:
        case EXPR_OP_GREATER:
        case EXPR_OP_LESS_EQUAL:
        case EXPR_OP_GREATER_EQUAL:
            return 3;
        case EXPR_OP_EQUAL:
        case EXPR_OP_NOT_EQUAL:
            return 2;
        case EXPR_OP_LOGICAL_AND:
            return 1;
        case EXPR_OP_LOGICAL_OR:
            return 0;
        default: fprintf(stderr, "Missing operator precedence: %d (operator enum)\n", op_kind); ASSERT(false);
    }
}

ASTExpression* parse_expression(ParserContext* context) {
    const TokenExt* tok = peek(0);

    // TODO: Parse annotations

    if (tok->kind == T_RETURN) {
        advance();

        ASTExpression_Return* out_expr = HEAP_ALLOC_OBJECT(ASTExpression_Return);
        out_expr->kind                 = EXPR_RETURN;
        out_expr->location             = location_from_token(tok);

        const TokenExt* tok0 = peek(0);
        ASTExpression* expr = NULL;
        if ((tok->flags & TF_POST_NEWLINE) || tok0->kind == ';') {
            // no expression
        } else {
            while (1) {
                expr = parse_expression(context);
                if (!expr) {
                    parse_error(tok0, "Expected an expression (if condition).");
                }

                array_push(&out_expr->exprs, &expr);

                tok0 = peek(0);
                if (tok0->kind == ',') {
                    advance();
                    continue;
                }
                break;
            }
        }

        return (ASTExpression*)out_expr;

    } else if (tok->kind == T_YIELD) {
        advance();

        ASTExpression_Yield* out_expr = HEAP_ALLOC_OBJECT(ASTExpression_Yield);
        out_expr->kind                = EXPR_YIELD;
        out_expr->location            = location_from_token(tok);

        const TokenExt* tok0 = peek(0);
        ASTExpression* expr = NULL;
        if ((tok->flags & TF_POST_NEWLINE) || tok0->kind == ';') {
            // no expression
        } else {
            while (1) {
                expr = parse_expression(context);
                if (!expr) {
                    parse_error(tok0, "Expected an expression (if condition).");
                }

                array_push(&out_expr->exprs, &expr);

                tok0 = peek(0);
                if (tok0->kind == ',') {
                    advance();
                    continue;
                }
                break;
            }
        }

        return (ASTExpression*)out_expr;

    } else if (tok->kind == T_IF) {
        advance();

        const TokenExt* tok0 = peek(0);
        ASTExpression* expr = parse_expression(context);
        if (!expr)
            parse_error(tok0, "Expected an expression (if condition).");

        const TokenExt* tok1 = peek(0);
        ASTExpression* body = parse_expression(context);
        if (!body)
            parse_error(tok1, "Expected an expression (if body).");
        
        ASTExpression* else_body = NULL;

        const TokenExt* tok2 = peek(0);
        if (tok2->kind == T_ELSE) {
            advance();
            const TokenExt* else_expr_tok = peek(0);
            else_body = parse_expression(context);
            if (!else_body) {
                parse_error(else_expr_tok, "Expected an expression (else body).");
            }
        }
        
        ASTExpression_If* out_expr = HEAP_ALLOC_OBJECT(ASTExpression_If);
        out_expr->kind             = EXPR_IF;
        out_expr->location         = location_from_token(tok);
        out_expr->condition_expr   = expr;
        out_expr->body_expr        = body;
        out_expr->else_expr        = else_body;

        return (ASTExpression*)out_expr;

    } else if (tok->kind == T_FOR) {
        advance();

        // @TODO Reverse annotation

        const TokenExt* tok0 = peek(0);
        const TokenExt* tok1 = peek(1);
        const TokenExt* tok2 = peek(2);
        const TokenExt* tok3 = peek(3);

        // for NR,IT in ITEMS BODY
        // for IT in ITEMS BODY
        // for ITEMS BODY

        string index_name;
        string item_name;

        if (tok0->kind == T_IDENTIFIER && tok1->kind == ',' && tok2->kind == T_IDENTIFIER && tok3->kind == T_IN) {
            advance();
            advance();
            advance();
            advance();
            cstring temp = DATA_FROM_IDENTIFIER(tok0);
            cstring temp2 = DATA_FROM_IDENTIFIER(tok2);
            item_name = string_clone_cstr(temp);
            index_name = string_clone_cstr(temp2);
        } else if (tok0->kind == T_IDENTIFIER && tok1->kind == T_IN) {
            advance();
            advance();

            cstring temp = DATA_FROM_IDENTIFIER(tok0);
            index_name = string_clone_cptr("nr");
            item_name = string_clone_cstr(temp);
        } else {
            index_name = string_clone_cptr("nr");
            item_name = string_clone_cptr("it");
        }

        ASTExpression* cond = parse_expression(context);
        ASTExpression* body = parse_expression(context);

        CREATE_EXPR(expr, ASTExpression_For, EXPR_FOR, tok);
        expr->index_name        = index_name;
        expr->item_name         =  item_name;
        expr->condition_expr    = cond;
        expr->body_expr         = body;

        return (ASTExpression*)expr;

    } else if (tok->kind == T_WHILE) {
        advance();
        
        ASTExpression* cond = NULL;
        
        const TokenExt* tok0 = peek(0);
        if (tok0->kind == '{') {
            // infinite loop, no condition
        } else {
            cond = parse_expression(context);
        }
        
        ASTExpression* body = parse_expression(context);

        CREATE_EXPR(expr, ASTExpression_While, EXPR_WHILE, tok);
        expr->condition_expr   = cond;
        expr->body_expr        = body;
        
        return (ASTExpression*)expr;

    // } else if (tok->kind == '#') {
    //     // compile time expression
    //     advance();

    //     if(IS_COMPTIME()) {
    //         parse_error(tok, "Nesting compile time expressions is not allowed.");
    //     }

    //     ComptimeKind compttime_kind = COMPTIME_NORMAL;

    //     tok = peek(0);
    //     if (tok->kind == T_IDENTIFIER) {
    //         cstring data = DATA_FROM_TOKEN(tok);
            
    //         if(string_equal_cstr(data, "emit")) {
    //             advance();
    //             compttime_kind = COMPTIME_EMIT;
    //         } else if(string_equal_cstr(data, "eval")) {
    //             advance();
    //             compttime_kind = COMPTIME_EVAL;
    //         }
    //     }

    //     enter_comptime_mode(context, compttime_kind);
    //     ASTExpression* node = parse_expression(context);
    //     exit_comptime_mode(context);

    //     // Find out from current imports which 

    //     // generate code
    } else if (tok->kind == T_CONTINUE) {
        advance();

        CREATE_EXPR(expr, ASTExpression_Continue, EXPR_CONTINUE, tok);
        
        return (ASTExpression*)expr;
    } else if (tok->kind == T_BREAK) {
        advance();
        
        CREATE_EXPR(expr, ASTExpression_Break, EXPR_BREAK, tok);
        
        return (ASTExpression*)expr;
    } else {
        
        Array_ASTExpressionP exprs;
        array_init(&exprs, 8);

        Array_ASTExpressionP ops;
        array_init(&ops, 8);

        bool expect_value = true;
        bool end_of_expression = false;

        while (true) {
            const TokenExt* tok0 = peek(0);
            const TokenExt* tok1 = peek(1);
            
            if (expect_value) {
            
                if (tok0->kind == T_IDENTIFIER) {
                    advance();

                    CREATE_EXPR(expr, ASTExpression_Identifier, EXPR_IDENTIFIER, tok0);

                    cstring name = DATA_FROM_IDENTIFIER(tok0);
                    expr->name = string_clone_cstr(name);

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0->kind == '[') {
                    advance();
                    
                    CREATE_EXPR(expr, ASTExpression_Initializer, EXPR_INITIALIZER, tok0);

                    while (true) {
                        const TokenExt* tok = peek(0);

                        if (tok->kind == ']') {
                            advance();
                            break;
                        }

                        ASTExpression_Initializer_Element element = {};
                        
                        if (tok->kind == T_IDENTIFIER && tok1->kind == '=') {
                            advance();
                            advance();
                            cstring name = DATA_FROM_IDENTIFIER(tok);
                            element.name = string_clone_cstr(name);
                        }

                        element.expr = parse_expression(context);
                        array_push(&expr->elements, &element);

                        tok = peek(0);
                        if (tok->kind == ']') {
                            continue;
                        } else if (tok->kind == ',') {
                            advance();
                            continue;
                        } else {
                            parse_error(tok, "Expected ',' or ']' to end or continue initializer.");
                        }
                    }

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0->kind == '{') {
                    ASTExpression* expr = parse_block_expression(context, false);

                    array_push(&exprs, &expr);
                } else if (tok0->kind == '(') {
                    advance();

                    ASTExpression* expr = parse_expression(context);

                    match(')');

                    array_push(&exprs, &expr);
                } else if (tok0->kind == T_LITERAL_INTEGER) {
                    advance();

                    CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0);
                    expr->literal_kind = EXPR_LITERAL_INTEGER;

                    cstring text = DATA_FROM_TOKEN(tok0);

                    // @TODO Parse 64-bit integer
                    // @TODO Hexidecimal
                    // Maybe integer token stores integer data instead of text form.
                    // if so it can be embedded in token.
                    expr->int_value = atoi(text.ptr);

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0->kind == T_LITERAL_FLOAT) {
                    advance();
                    ASSERT((false,"parse literal float"));
                } else if (tok0->kind == T_LITERAL_STRING) {
                    advance();
                    CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0);
                    expr->literal_kind = EXPR_LITERAL_FLOAT;

                    cstring text = DATA_FROM_STRING(tok0);
                    expr->string_value = string_clone_cstr(text);

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0->kind == '-') {
                    advance();

                    CREATE_EXPR(expr, ASTExpression_Unary, EXPR_UNARY, tok0);
                    expr->op_kind = EXPR_OP_SUB;

                    array_push(&ops, (ASTExpression**)&expr);
                    continue;
                } else {
                    parse_error(tok0, "Not a valid token for expression (primary value)");
                }
                expect_value = false;
                continue;
            } else {
                ASTExpression_Unary* unary_op = NULL;
                if (tok0->kind == '&' && !HAS_PRE_WHITESPACE(tok0)) {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0);
                    unary_op->op_kind = EXPR_OP_ADDRESS_OF;
                } else if (tok0->kind == '^' && !HAS_PRE_WHITESPACE(tok0)) {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0);
                    unary_op->op_kind = EXPR_OP_DEREF;
                } else if (tok0->kind == '~') {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0);
                    unary_op->op_kind = EXPR_OP_BITWISE_NEGATE;
                } else if (tok0->kind == '!') {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0);
                    unary_op->op_kind = EXPR_OP_LOGICAL_NOT;
                }
                if (unary_op) {
                    advance();
                    unary_op->expr = exprs.ptr[exprs.len-1];
                    exprs.ptr[exprs.len-1] = (ASTExpression*)unary_op;
                    continue;
                }

                ASTExpression_Binary* operation = NULL;
                
                if (tok0->kind == '+') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_ADD;
                } else if (tok0->kind == '-') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_SUB;
                } else if (tok0->kind == '*') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_MUL;
                } else if (tok0->kind == '/') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_DIV;
                } else if (tok0->kind == '%') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_MODULO;
                } else if (tok0->kind == '&' && tok1->kind == '&') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_LOGICAL_AND;
                } else if (tok0->kind == '|' && tok1->kind == '|') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_LOGICAL_OR;
                } else if (tok0->kind == '=' && tok1->kind == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_EQUAL;
                } else if (tok0->kind == '!' && tok1->kind == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_NOT_EQUAL;
                } else if (tok0->kind == '<' && tok1->kind == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_LESS_EQUAL;
                } else if (tok0->kind == '>' && tok1->kind == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_GREATER_EQUAL;
                } else if (tok0->kind == '<' && tok1->kind == '<') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_BITWISE_LSHIFT;
                } else if (tok0->kind == '>' && tok1->kind == '>') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_BITWISE_RSHIFT;
                } else if (tok0->kind == '<') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_LESS;
                } else if (tok0->kind == '>') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_GREATER;
                } else if (tok0->kind == '^') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_BITWISE_XOR;
                } else if (tok0->kind == '&') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_BITWISE_AND;
                } else if (tok0->kind == '|') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0);
                    operation->op_kind = EXPR_OP_BITWISE_OR;
                } else if (tok0->kind == '.') {
                    advance();
                    const TokenExt* tok = match(T_IDENTIFIER);

                    cstring name = DATA_FROM_IDENTIFIER(tok);
                    CREATE_EXPR(expr, ASTExpression_Member, EXPR_MEMBER, tok0);
                    expr->name = string_clone_cstr(name);

                    ASTExpression* last_expr = array_last(&exprs);
                    expr->expr = last_expr;
                    exprs.ptr[exprs.len-1] = (ASTExpression*)expr;
                    continue;
                } else if (tok0->kind == ':') {
                    advance();
                    CREATE_EXPR(expr, ASTExpression_Cast, EXPR_CAST, tok0);
                    
                    expr->type_name = parse_type(context);

                    ASTExpression* last_expr = array_last(&exprs);
                    expr->expr = last_expr;
                    exprs.ptr[exprs.len-1] = (ASTExpression*)expr;
                    continue;
                } else {
                    Array_ASTExpression_Call_PolyArgument polyargs = {};
                    
                    bool parsed_poly_args = false;
                    if (tok0->kind == '<' && !HAS_PRE_WHITESPACE(tok0)) {
                        advance();
                        parsed_poly_args = true;
                        
                        while (true) {
                            const TokenExt* tok = peek(0);
                            if (tok->kind == '>') {
                                advance();
                                break;
                            }
                            ASTExpression_Call_PolyArgument arg = {};
                            arg.expr = parse_expression(context);
                            array_push(&polyargs, &arg);

                            tok = peek(0);

                            if (tok->kind == '>') {
                                continue;
                            } else if (tok->kind == ',') {
                                advance();
                                continue;
                            } else {
                                parse_error(tok, "Expected '>' or ',' to continue or end polymorphic arguments.");
                            }
                        }
                    }

                    if (tok0->kind == '(' || parsed_poly_args) {
                        match('(')
                        CREATE_EXPR(expr, ASTExpression_Call, EXPR_CALL, tok0);
                        expr->polymorphic_args = polyargs;
                        
                        while (true) {
                            const TokenExt* tok = peek(0);
                            if (tok->kind == ')') {
                                advance();
                                break;
                            }
                            
                            ASTExpression_Call_Argument arg = {};

                            const TokenExt* tok1 = peek(1);

                            if (tok->kind == T_IDENTIFIER && tok1->kind == '=') {
                                advance();
                                advance();
                                cstring name = DATA_FROM_IDENTIFIER(tok);
                                arg.name = string_clone_cstr(name);
                            }

                            arg.expr = parse_expression(context);
                            array_push(&expr->arguments, &arg);
                            
                            tok = peek(0);

                            if (tok->kind == ')') {
                                continue;
                            } else if (tok->kind == ',') {
                                advance();
                                continue;
                            } else {
                                parse_error(tok, "Expected ')' or ',' to continue or end function arguments.");
                            }
                        }

                        ASTExpression* last_expr = array_last(&exprs);
                        expr->expr = last_expr;
                        exprs.ptr[exprs.len-1] = (ASTExpression*)expr;
                        continue;
                    }

                    end_of_expression = true;
                }
                if (operation) {
                    advance();
                    array_push(&ops, (ASTExpression**)&operation);
                }
                expect_value = true;
            }

            while (true) {
                if (ops.len == 0) {
                    break;
                }

                ASTExpression_Binary* op_last  = (ASTExpression_Binary*)array_get(&ops, ops.len-1);
                ASTExpression_Binary* op_last2 = ops.len >= 2 ? (ASTExpression_Binary*)array_get(&ops, ops.len-2) : NULL;

                if (op_last->kind == EXPR_UNARY && op_last->op_kind == EXPR_OP_SUB) {
                    ASTExpression_Unary* op = (ASTExpression_Unary*)op_last;
                    op->expr = exprs.ptr[exprs.len-1];
                    exprs.ptr[exprs.len-1] = (ASTExpression*)op;
                    array_pop(&ops);
                    continue;
                }

                int op_prec = op_precedence(op_last->op_kind);
                int op_prec2 = ops.len >= 2 ? op_precedence(op_last2->op_kind) : -1;

                if (op_last2 && op_prec2 >= op_prec) {
                    // COMBINE OP LAST2
                    ASSERT(exprs.len >= 2);

                    ASTExpression* expr0 = array_get(&exprs, exprs.len-1);
                    ASTExpression* expr1 = array_get(&exprs, exprs.len-2);

                    op_last2->right = expr0;
                    op_last2->left = expr1;
                    exprs.ptr[exprs.len-2] = (ASTExpression*)op_last2;

                    array_pop(&exprs);
                    ops.ptr[ops.len-2] = (ASTExpression*)op_last;
                    array_pop(&ops);

                } else if (end_of_expression) {
                    // COMBINE OP LAST
                    ASSERT(exprs.len >= 2);

                    ASTExpression* expr0 = array_get(&exprs, exprs.len-1);
                    ASTExpression* expr1 = array_get(&exprs, exprs.len-2);

                    op_last->right = expr0;
                    op_last->left = expr1;
                    exprs.ptr[exprs.len-2] = (ASTExpression*)op_last;

                    array_pop(&exprs);
                    array_pop(&ops);
                } else {
                    break;
                }
            }

            if (end_of_expression) {
                ASSERT(exprs.len == 1);
                break;
            }
        }

        return exprs.ptr[0];
    }
}


ASTFunction* parse_function(ParserContext* context) {
    match(T_FN);

    const TokenExt* tok = match(T_IDENTIFIER);
    check_error_ext(tok, "Expected an identifier.");
        
    cstring name = DATA_FROM_IDENTIFIER(tok);

    ASTFunction* out_function = HEAP_ALLOC_OBJECT(ASTFunction);
    out_function->name      = string_clone_cstr(name);
    out_function->location  = location_from_token(tok);

    match('(');

    // Parse arguments
    while (true) {

        const TokenExt* tok = peek(0);
        if (tok->kind == ')') {
            advance();
            break;
        }

        if (tok->kind == T_IDENTIFIER) {
            advance();

            match(':');

            ASTType type_name = parse_type(context);

            const TokenExt* tok = peek(0);
            if (tok->kind == ';') {
                advance();
            }
            ASTFunction_Parameter parameter;
            cstring field_name      = DATA_FROM_IDENTIFIER(tok);
            parameter.name          = string_clone_cstr(field_name);
            parameter.location      = location_from_token(tok);
            parameter.default_value = NULL;
            parameter.type_name     = type_name;

            array_push(&out_function->parameters, &parameter);
        } else {
            parse_error(tok, "Expected a parameter, 'item : i32;'");
        }
        if (tok->kind == ',') {
            advance();
            continue;
        } else if(tok->kind == ')') {
            continue;
        } else {
            parse_error(tok, "Expected a ')' or ','");
        }
    }

    // Parse return values
    const TokenExt* tok0 = peek(0);
    const TokenExt* tok1 = peek(1);
    if (tok0->kind == '-' && tok1->kind == '>') {
        advance();
        advance();
        while (true) {

            const TokenExt* tok0 = peek(0);
            const TokenExt* tok1 = peek(1);

            ASTFunction_Parameter parameter;
            parameter.location = location_from_token(tok);

            if (tok0->kind == T_IDENTIFIER && tok1->kind == ':') {
                advance();
                advance();

                const TokenExt* tok = peek(0);
                if (tok->kind == ';') {
                    advance();
                }
                cstring field_name     = DATA_FROM_IDENTIFIER(tok0);
                parameter.name         = string_clone_cstr(field_name);
                parameter.default_value = NULL;

                array_push(&out_function->return_values, &parameter);
                match(':');
            }

            ASTType type_name = parse_type(context);
            parameter.type_name = type_name;
            
            if (tok->kind == ')') {
                advance();
                break;
            } else if (tok->kind == ',') {
                advance();
                continue;
            } else {
                parse_error(tok, "Expected a return value, '-> i32, i32'");
            }
        }
    }

    const TokenExt* tok_body = peek(0);
    if (tok_body->kind == '{') {
        ASTExpression* body = parse_block_expression(context, false);
        out_function->body = body;
    }

    return out_function;
}

ASTStruct* parse_struct(ParserContext* context) {
    match(T_STRUCT);

    advance();

    const TokenExt* tok = match(T_IDENTIFIER);
    check_error_ext(tok, "Expected an identifier.");
        
    cstring name = DATA_FROM_IDENTIFIER(tok);

    ASTStruct* out_struct = HEAP_ALLOC_OBJECT(ASTStruct);
    out_struct->name      = string_clone_cstr(name);
    out_struct->location  = location_from_token(tok);

    match('{');

    while (true) {

        const TokenExt* tok = peek(0);

        if (tok->kind == T_IDENTIFIER) {
            advance();

            match(':');

            ASTType type_name = parse_type(context);

            const TokenExt* tok = peek(0);
            if (tok->kind == ';') {
                advance();
            }
            ASTStruct_Field field;
            cstring field_name = DATA_FROM_IDENTIFIER(tok);
            field.name         = string_clone_cstr(field_name);
            field.location     = location_from_token(tok);
            field.type_name    = type_name;

            array_push(&out_struct->fields, &field);
        } else if (tok->kind == '}') {
            advance();
            break;
        } else {
            parse_error(tok, "Expected a field, 'item : i32;'");
        }
        // @TODO Parse function
    }

    return out_struct;
}


ASTType parse_type(ParserContext* context) {
    const TokenExt* tok = peek(0);

    ASTType type_name;

    if (tok->kind == T_IDENTIFIER) {
        cstring str = DATA_FROM_IDENTIFIER(tok);
        type_name = string_clone_cstr(str);
    } else {
        parse_error(tok, "Invalid type.");
    }
    return type_name;
}
