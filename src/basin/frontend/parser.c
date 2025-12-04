/*
    The parsing and matching pretty much always uses TokenExt since it encompasses
    Token and TokenExt.
*/

#include "basin/frontend/parser.h"

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
    TokenStream* stream;
    int head; // token head
    
    ComptimeKind comptime_kind;
    
    jmp_buf jump_state;
    TokenExt bad_token; // token causing bad syntax
    char error_message[256];
} ParserContext;

static const TokenExt* _match(ParserContext* context, TokenKind kind) {
    if (context->head < context->stream->tokens_len && context->stream->tokens[context->head].kind == (kind)) {
        const TokenExt* tok = (const TokenExt*)&context->stream->tokens[context->head];
        context->head += 1 + (TOKEN_PER_EXT_TOKEN - 1) * IS_EXT_TOKEN(kind);
        return tok;
    } else {
        parse_error(context, (const TokenExt*)&context->stream->tokens[context->head], "Unexpected token");
        return &EOF_TOKEN_EXT;
    }
}

static const TokenExt* _peek(ParserContext* context, int n) {
    int head = context->head;
    const TokenExt* tok;
    while(n--) {
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

ASTExpression* parse_expression(ParserContext* context);



Result parse_stream(TokenStream* stream, AST** out_ast) {
    // We implement this recursively because it's easier to debug issues
    Result result;
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    AST* ast = HEAP_ALLOC_OBJECT(AST);

    ParserContext context = {0};
    context.stream = stream;
    context.head = 0;

    int res = setjmp(context.jump_state);

    if (res == 0) {

        // parse multiple expressions
        parse_expression(&context);

        *out_ast = ast;
    } else {
        int line, column;
        string code;
        bool yes = compute_source_info(stream, context.bad_token, &line, &column, &code);

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "\033[31m%s:%d:%d:\033[0m %s\n%s", stream->import->path.ptr, line, column, context.error_message, code.ptr);
        result.kind = FAILURE;
        result.message = alloc_string(buffer);
        // fprintf(stderr, "Parser error %s\n", stream->import->path.ptr);
    }


    return result;
}

#define check_error_ext(T, ...) ((T)->kind == T_END_OF_FILE ? parse_error(context, (const TokenExt*)(void*)(T), __VA_ARGS__) : 0)

void parse_error(ParserContext* context, const TokenExt* tok, char* fmt, ...) {
    context->bad_token = *(const TokenExt*)tok;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(context->error_message, sizeof(context->error_message), fmt, ap);
    va_end(ap);

    longjmp(context->jump_state, 1);
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

ASTExpression* parse_expression(ParserContext* context) {
    const TokenExt* tok = peek(0);

    // TODO: Parse annotations

    if(tok->kind == T_IMPORT) {
        advance();

        if(IS_COMPTIME()) {
            parse_error(context, tok, "Import is not allowed in compile time expression. Use 'compiler_import(\"windows\")' from 'import \"compiler\"' instead.");
        }

        const TokenExt* tok = match(T_LITERAL_STRING);
        check_error_ext(tok, "Expected a string");

        // driver_submit_tokenize_task with import id, refer to it?

        // return expression?

        // TODO: Parse as

    } else if (tok->kind == T_GLOBAL) {
        advance();
        
        if(IS_COMPTIME()) {
            parse_error(context, tok, "Global variables are not allowed inside compile time expressions.");
        }

        const TokenExt* tok = match(T_IDENTIFIER);
        check_error_ext(tok, "Expected an identifier.");
            
        string name = DATA_FROM_TOKEN(tok);

        match(':');


    } else if (tok->kind == T_RETURN) {
        advance();

    } else if (tok->kind == T_IF) {
        advance();

    } else if (tok->kind == T_FOR) {
        advance();
        
    } else if (tok->kind == '#') {
        // compile time expression
        advance();

        if(IS_COMPTIME()) {
            parse_error(context, tok, "Nesting compile time expressions is not allowed.");
        }

        ComptimeKind compttime_kind = COMPTIME_NORMAL;

        tok = peek(0);
        if (tok->kind == T_IDENTIFIER) {
            string data = DATA_FROM_TOKEN(tok);
            
            if(string_equal_cstr(data, "emit")) {
                advance();
                compttime_kind = COMPTIME_EMIT;
            } else if(string_equal_cstr(data, "eval")) {
                advance();
                compttime_kind = COMPTIME_EVAL;
            }
        }

        enter_comptime_mode(context, compttime_kind);
        ASTExpression* node = parse_expression(context);
        exit_comptime_mode(context);

        // Find out from current imports which 

        // generate code


    } else {
        parse_error(context, tok, "Unknown token.");
    }

    return NULL;
}