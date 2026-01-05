
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

    ASTExpression_Block* current_block;

    TypeInfo* inferred_type;

    int registers[256];
    
    jmp_buf jump_state;
    SourceLocation bad_location;
    CLocation c_location;
    char error_message[512];
} GenIRContext;

typedef struct IRValue {
    int regnum;
} IRValue;

int allocate_register(GenIRContext* context) {
    // @TODO Optimize
    int max = sizeof(context->registers)/sizeof(*context->registers);
    for (int i=0;i<max;i++) {
        if (context->registers[i] == 0)
            return i;
    }
    ASSERT(false); // compiler bug, or rather flaw
    return -1;
}

void free_register(GenIRContext* context, int regnum) {
    ASSERT(context->registers[regnum] != 0);
    context->registers[regnum] = 0;
}

typedef enum GenFlags {
    GEN_NONE         = 0x0,
    GEN_IGNORE_VALUE = 0x1,
    GEN_INFER_TYPE   = 0x2,
} GenFlags;

void walk(GenIRContext* context, ASTExpression* _expr);
void generate_function(GenIRContext* context, ASTFunction* func);
IRValue generate_expression(GenIRContext* context, ASTExpression* expr, GenFlags flags);

#define gen_error(LOC, FMT, ...) ( context->c_location.path = __FILE__, context->c_location.line = __LINE__, _gen_error(context, LOC, FMT __VA_OPT__(,)  __VA_ARGS__) )

void _gen_error(GenIRContext* context, SourceLocation loc, char* fmt, ...) {
    context->bad_location = loc;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(context->error_message, sizeof(context->error_message), fmt, ap);
    va_end(ap);

    longjmp(context->jump_state, 1);
}

Result generate_ir(Driver* driver, AST* ast, IRCollection* collection) {

    // Find functions and generate them
    // We implement this recursively because it's easier to debug issues
    Result result = {};
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    IRFunction func = {};
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
        int len = 0;
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[0;30m%s:%d\033[0m\n", context.c_location.path, context.c_location.line);
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[31m%s:%d:%d:\033[0m %s\n%s", ast->stream->import->path.ptr, line, column, context.error_message, code.ptr);
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
    
    if(func->body) {
        printf(" skip no body %s\n", func->name.ptr);
        return;
    }

    // init_builder(func);

    // generate_prologue(func);
    // allocate stack space for local variables
    // temporary variables, argument passed on stack.
    // maybe determine this after IR has been generated?
    
    generate_expression(context, func->body, GEN_NONE);
    
    // generate_epilog(func);

    // fini_builder(func);
}


IRValue generate_expression(GenIRContext* context, ASTExpression* _expression, GenFlags flags) {
    IRValue ir_value = {};
    switch (_expression->kind) {
        case EXPR_BLOCK: {
            ASTExpression_Block* expression = (ASTExpression_Block*) _expression;
            for (int i=0;i<expression->expressions.len;i++) {
                ASTExpression_Block* prev_block = context->current_block;
                context->current_block = expression;
                
                generate_expression(context, expression->expressions.ptr[i], GEN_IGNORE_VALUE);

                context->current_block = prev_block;
            }
        } break;
        case EXPR_LITERAL: {
            ASTExpression_Literal* expression = (ASTExpression_Literal*) _expression;
            switch (expression->literal_kind) {
                case EXPR_LITERAL_INTEGER: {
                    // @TODO Don't assume type
                    //   Get type from parent expression?
                    if (context->inferred_type) {
                        ASSERT(false);
                    } else {
                        int reg = allocate_register(&context);
                        ir_imm32(&context->builder, reg, expression->int_value, IR_TYPE_SINT32);
                        ir_value.regnum = reg;
                    }
                } break;
                case EXPR_LITERAL_FLOAT: {
                    // @TODO Don't assume type
                    //   Get type from parent expression?
                    if (context->inferred_type) {
                        ASSERT(false);
                    } else {
                        int reg = allocate_register(&context);
                        float v = expression->float_value;
                        ir_imm32(&context->builder, reg, *(i32*)&v, IR_TYPE_FLOAT32);
                        ir_value.regnum = reg;
                    }
                } break;
                case EXPR_LITERAL_STRING: {
                    // WHAT THE HELL DO WE DO HERE?
                    // ALLOCATE TWO REG NUMS?
                    // WHAT ABOUT A STRUCT WITH 10 FIELDS?
                    // WE CAN'T ALLOCATE AND RETURN 10 REG NUMS!
                    // SO PUT IT ON THE STACK?
                    // i guess
                    ASSERT(false);
                } break;
                default: ASSERT(false);
            }
        } break;
        case EXPR_MEMBER: {

        } break;
        case EXPR_IDENTIFIER: {

        } break;
        case EXPR_RETURN: {

        } break;
        case EXPR_BINARY: {

        } break;
        case EXPR_CALL: {
            ASTExpression_Call* expr_call = (ASTExpression_Call*) _expression;
            // @TODO Handle polymorphic arguments
            ASSERT(expr_call->polymorphic_args.len == 0);
            // @TODO Handle calling function pointers
            ASSERT(expr_call->expr->kind == EXPR_IDENTIFIER);
            
            ASTExpression_Identifier* expr_ident = (ASTExpression_Identifier*) expr_call->expr;

            // @TODO Implement find_function. special stuff for overloading etc. ?
            FindResult result = {};
            bool res = find_identifier(cstr(expr_ident->name), context->ast, context->current_block, &result);
            if (!res) {
                gen_error(expr_ident->location, "Could not find '%s'", expr_ident->name.ptr);
            }
            
            if (result.kind != FOUND_FUNCTION) {
                // @TODO Print what kind we found (struct, global, etc)
                gen_error(expr_ident->location, "Cannot call non-function '%s'", expr_ident->name.ptr);
            }
            
            ASTFunction* func = result.f_function;
            ASSERT(func);

            IROperand operands[20];
            int arg_count = func->parameters.len;
            int ret_count = func->return_values.len;
            ASSERT(arg_count + ret_count <= sizeof(operands)/sizeof(*operands));

            for (int i=0;i<expr_call->arguments.len;i++) {
                ASTExpression_Call_Argument* arg = &expr_call->arguments.ptr[i];

                int param_index = i;
                if (arg->name.len) {
                    param_index = find_function_parameter(cstr(arg->name), func);
                    if (param_index == -1) {
                        gen_error(arg->location, "Function '%s' does not have parameter '%s', mispelled?", func->name.ptr, arg->name.ptr);
                    }
                }

                ASTFunction_Parameter* param = &func->parameters.ptr[param_index];

                // @TODO Infer type from param
                IRValue arg_value = generate_expression(context, arg->expr, GEN_NONE);

                operands[param_index] = arg_value.regnum;
            }

            for (int i=0;i<func->return_values.len;i++) {
                ASTFunction_Parameter* param = &func->return_values.ptr[i];
                operands[arg_count + i] = allocate_register(&context);
            }

            IRFunction_id id = 0;
            
            ir_call(&context->builder, id, func->parameters.len, func->return_values.len, operands);

            // @TODO How to return multiple IR values?
            //    Only assignment allows multiple IR values

        } break;
        default: ASSERT(false);
    }

    return ir_value;
}
