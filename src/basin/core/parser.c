#include "basin/core/parser.h"

#include "platform/memory.h"

#include <setjmp.h>
#include <stdarg.h>

#define match(KIND) (context->head >= context->stream->tokens_len ? false : (context->stream->tokens[context->head].kind == (KIND) ? (context->head++, true) : false ))
#define match_ext(KIND) (context->head+1 >= context->stream->tokens_len ? &EOF_TOKEN_EXT : (context->stream->tokens[context->head].kind == (KIND) ? (TokenExt*)&context->stream->tokens[context->head+=2] : &EOF_TOKEN_EXT) )

// 0 current token
#define peek(N) (context->head >= context->stream->tokens_len ? &EOF_TOKEN : &context->stream->tokens[context->head + N])

#define advance() (context->head >= context->stream->tokens_len ? &EOF_TOKEN : &context->stream->tokens[context->head++])

typedef struct {
    TokenStream* stream;
    int head; // token head

    jmp_buf jump_state;

    TokenExt bad_token; // token causing bad syntax
    char error_message[256];
} ParserContext;


void parse_expression(ParserContext* context);



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

#define check_error_ext(T, ...) ((T)->kind == T_END_OF_FILE ? parse_error(context, (Token*)(void*)(T), __VA_ARGS__) : 0)

void parse_error(ParserContext* context, const Token* tok, char* fmt, ...) {
    context->bad_token = *(const TokenExt*)tok;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(context->error_message, sizeof(context->error_message), fmt, ap);
    va_end(ap);

    longjmp(context->jump_state, 1);
}

void parse_expression(ParserContext* context) {
    const Token* tok = peek(0);

    // TODO: Parse annotations

    if(tok->kind == T_IMPORT) {
        advance();

        const TokenExt* tok = match_ext(T_LITERAL_STRING);
        check_error_ext(tok, "Expected a string");

        // return expression?

        // TODO: Parse as

    // } else if (tok.kind == T_FN) {

    } else {
        parse_error(context, tok, "Unknown token.");
    }
}