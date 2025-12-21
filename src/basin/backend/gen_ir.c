
#include "basin/backend/gen_ir.h"

#include "basin/core/driver.h"
#include "basin/frontend/ast.h"
#include "basin/backend/ir.h"

#include "platform/memory.h"

#include <setjmp.h>
#include <stdarg.h>


typedef struct {
    Driver* driver;
    AST* ast;
    IRBuilder builder;
    
    jmp_buf jump_state;
    SourceLocation bad_location;
    char error_message[256];
} GenIRContext;

void walk(GenIRContext* context, ASTExpression* _expr);
void generate_function(GenIRContext* context, ASTFunction* func);

Result generate_ir(Driver* driver, AST* ast, IRCollection* collection) {

    // Find functions and generate them
    // We implement this recursively because it's easier to debug issues
    Result result;
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    IRFunction func;
    // int ok;

    atomic_array_push(&driver->collection->functions, &func);
    // atomic_array_push(&driver->collection->functions, &ok);


    GenIRContext context = {0};
    context.driver = driver;
    context.ast = ast;
    context.builder.collection = driver->collection;

    int res = setjmp(context.jump_state);

    if (res == 0) {

        walk(&context, ast->global_block);

    } else {
        int line, column;
        string code;
        bool yes = compute_source_info(ast->stream, context.bad_location, &line, &column, &code);

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "\033[31m%s:%d:%d:\033[0m %s\n%s", ast->stream->import->path.ptr, line, column, context.error_message, code.ptr);
        result.kind = FAILURE;
        result.message = string_clone_cptr(buffer);
    }

    return result;
}


void walk(GenIRContext* context, ASTExpression* _expr) {
    switch (_expr->kind) {
        case EXPR_BLOCK: {
            ASTExpression_Block* expr = (ASTExpression_Block*) _expr;
            for (int i=0;i<expr->functions.len;i++) {
                ASTFunction* func = expr->functions.ptr[i];
                generate_function(context, func);
            }
        }
    }
}


void generate_function(GenIRContext* context, ASTFunction* func) {
    printf("Gen Func %s\n", func->name.ptr);

    // context->builder.collection->functions
}
