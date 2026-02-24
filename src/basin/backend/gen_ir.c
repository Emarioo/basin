
#include "basin/backend/gen_ir.h"

#include "basin/core/driver.h"
#include "basin/frontend/ast.h"
#include "basin/backend/ir.h"

#include "platform/platform.h"
#include "platform/platform.h"

#include <setjmp.h>
#include <stdarg.h>


#define PROFILE_START() TracyCZone(zone, 1); push_profile_zone(context, zone)

#define PROFILE_END() TracyCZoneEnd(zone); pop_profile_zone(context)


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
    
    // Zones for tracy are cleaned up when long jumping.
    int zones_cap;
    int zones_len;
    TracyCZoneCtx* zones;
} GenIRContext;

typedef struct IRValue {
    int regnum;
} IRValue;


static inline void cleanup_profile_zones(GenIRContext* ctx) {
    while (ctx->zones_len) {
        ctx->zones_len--;
        TracyCZoneEnd(ctx->zones[ctx->zones_len]);
    }
}

static inline void push_profile_zone(GenIRContext* ctx, TracyCZoneCtx zone) {
    if (ctx->zones_len + 1 >= ctx->zones_cap) {
        int new_cap = ctx->zones_cap * 2 + 100;
        void* new_zones = mem__allocate(new_cap, ctx->zones);
        ctx->zones = new_zones;
        ctx->zones_cap = new_cap;
    }
    ctx->zones[ctx->zones_len] = zone;
    ctx->zones_len++;
}
static inline void pop_profile_zone(GenIRContext* ctx) {
    ASSERT(ctx->zones_len > 0);
    ctx->zones_len--;
}

int allocate_register(GenIRContext* context) {
    // @TODO Optimize
    int max = sizeof(context->registers)/sizeof(*context->registers);
    for (int i=1;i<max;i++) {
        if (context->registers[i] == 0) {
            context->registers[i] = 1;
            return i;
        }
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


Result generate_ir(Driver* driver, AST* ast, IRProgram* program) {
    TracyCZone(zone, 1);
    // Find functions and generate them
    // We implement this recursively because it's easier to debug issues
    Result result = {};
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    IRFunction func = {};

    // @TODO Comp time function for AST
    // atomic_array_push(&driver->program->functions, &func);


    GenIRContext context = {0};
    context.driver = driver;
    context.ast = ast;
    context.builder.program = driver->program;

    int res = setjmp(context.jump_state);

    if (res == 0) {

        walk(&context, (ASTExpression*)ast->global_block);

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

    TracyCZoneEnd(zone);
    return result;
}

u32 submit_rodata_string(GenIRContext* context, cstring str) {
    u32 prev_offset = atomic_add64(&context->driver->section_rodata->data_len, str.len+1);
    memcpy(context->driver->section_rodata->data, str.ptr, str.len);
    context->driver->section_rodata->data[0] = '\0';
    return prev_offset;
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
    PROFILE_START();
    printf("Gen Func %s\n", func->name.ptr);
    
    if(!func->body) {
        printf(" skip no body %s\n", func->name.ptr);
        goto end;
    }

    IRFunction* ir_func;
    {
        IRFunction empty_func = {};
        ir_func = atomic_array_getptr(&context->driver->program->functions, func->ir_function_id);

        // @TODO Init_builder(func);
        context->builder.function = ir_func;
    }

    // generate_prologue(func);

    // allocate stack space for local variables
    // temporary variables, argument passed on stack.
    // maybe determine this after IR has been generated?
    
    generate_expression(context, func->body, GEN_NONE);
    
    // generate_epilog(func);

    // fini_builder(func);

    print_ir_function(context->driver->program, ir_func);

end:
    PROFILE_END();
}

IRValue generate_reference(GenIRContext* context, ASTExpression* _expression) {
    PROFILE_START();
    IRValue ir_value = {};
    IRBuilder* builder = &context->builder;
    switch (_expression->kind) {
        case EXPR_IDENTIFIER: {
            ASTExpression_Identifier* iden = (ASTExpression_Identifier*)_expression;
            FindResult result = {};
            bool yes = find_identifier(cstr(iden->name), context->ast, context->current_block, &result);
            switch (result.kind) {
                case FOUND_VARIABLE: {
                    ir_value.regnum = allocate_register(context);
                    int offset = 4;
                    ir_address_of_variable(builder, ir_value.regnum, SECTION_ID_STACK, offset);
                } break;
                // case FOUND_GLOBAL:
                // case FOUND_CONSTANT:
                // case FOUND_FUNCTION:
                // case FOUND_IMPORT:
                // case FOUND_LIBRARY:
                // case FOUND_STRUCT:
                // case FOUND_ENUM:
                // case FOUND_ENUM_MEMBER:

                default: ASSERT(false);
            }
        }break;
        default: ASSERT(false);
    }

    PROFILE_END();
    return ir_value;
}


IRValue generate_expression(GenIRContext* context, ASTExpression* _expression, GenFlags flags) {
    PROFILE_START();
    IRValue ir_value = {};
    IRBuilder* builder = &context->builder;
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
                        int reg = allocate_register(context);
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
                        int reg = allocate_register(context);
                        float v = expression->float_value;
                        ir_imm32(&context->builder, reg, *(i32*)&v, IR_TYPE_FLOAT32);
                        ir_value.regnum = reg;
                    }
                } break;
                case EXPR_LITERAL_STRING: {
                    // int offset = 8;
                    
                    // find offset to string in .rodata section

                    // int reg = allocate_register(context);
                    // ir_imm32(builder, reg, offset, IR_TYPE_SINT32);

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
            ASTExpression_Member* expr_member = (ASTExpression_Member*)_expression;

            if (expr_member->expr->kind == EXPR_IDENTIFIER) {
                ASTExpression_Identifier* expr_identifier = (ASTExpression_Identifier*)expr_member->expr;
                FindResult result = {};
                bool yes = find_identifier(cstr(expr_identifier->name), context->ast, context->current_block, &result);
                switch (result.kind) {
                    case FOUND_VARIABLE: {
                        ir_value.regnum = allocate_register(context);
                        int var_offset = 16;
                        ir_address_of_variable(builder, ir_value.regnum, SECTION_ID_STACK, var_offset);
                        int field_offset = 6;
                        ir_load(builder, ir_value.regnum, ir_value.regnum, field_offset, IR_TYPE_SINT64);
                    } break;
                    // case FOUND_GLOBAL:
                    // case FOUND_CONSTANT:
                    // case FOUND_FUNCTION:
                    // case FOUND_IMPORT:
                    // case FOUND_LIBRARY:
                    // case FOUND_STRUCT:
                    // case FOUND_ENUM:
                    // case FOUND_ENUM_MEMBER:
                    case FOUND_NONE: {
                        gen_error(expr_identifier->location, "Could not find '%s'", expr_identifier->name.ptr);
                    } break;
                    default: ASSERT(false);
                }
            } else {
                ASSERT(false);
            }
        } break;
        case EXPR_IDENTIFIER: {
            ASTExpression_Identifier* expr_identifier = (ASTExpression_Identifier*)_expression;
            FindResult result = {};
            bool yes= find_identifier(cstr(expr_identifier->name), context->ast, context->current_block, &result);
            switch (result.kind) {
                case FOUND_VARIABLE:{
                    ir_value.regnum = allocate_register(context);
                    int var_offset = 16;
                    ir_address_of_variable(builder, ir_value.regnum, SECTION_ID_STACK, var_offset);
                    int field_offset = 6;
                    ir_load(builder, ir_value.regnum, ir_value.regnum, field_offset, IR_TYPE_SINT64);
                } break;
                // case FOUND_GLOBAL:
                // case FOUND_CONSTANT:
                // case FOUND_FUNCTION:
                // case FOUND_IMPORT:
                // case FOUND_LIBRARY:
                // case FOUND_STRUCT:
                // case FOUND_ENUM:
                // case FOUND_ENUM_MEMBER:
                case FOUND_NONE: {
                    gen_error(expr_identifier->location, "Could not find '%s'", expr_identifier->name.ptr);
                } break;
                default: ASSERT(false);
            }
        } break;
        case EXPR_RETURN: {
            ASTExpression_Return* expr_return = (ASTExpression_Return*)_expression;

            IROperand operands[10];
            for (int i=0;i<expr_return->exprs.len;i++) {
                IRValue value = generate_expression(context, expr_return->exprs.ptr[i], 0);
                operands[i] = value.regnum;
            }
            ir_ret(builder, expr_return->exprs.len, operands);
        } break;
        case EXPR_BINARY: {
            ASTExpression_Binary* expr_binary = (ASTExpression_Binary*) _expression;

            IRValue left_value = generate_expression(context, expr_binary->left, 0);
            IRValue right_value = generate_expression(context, expr_binary->right, 0);

            ir_value.regnum = allocate_register(context);

            switch (expr_binary->op_kind) {
                case EXPR_OP_ADD: {
                    ir_add(builder, ir_value.regnum, left_value.regnum, right_value.regnum, IR_TYPE_SINT64);
                } break;
                case EXPR_OP_SUB: {
                    ir_sub(builder, ir_value.regnum, left_value.regnum, right_value.regnum, IR_TYPE_SINT64);
                } break;
                case EXPR_OP_MUL: {
                    ir_mul(builder, ir_value.regnum, left_value.regnum, right_value.regnum, IR_TYPE_SINT64);
                } break;
                case EXPR_OP_DIV: {
                    ir_div(builder, ir_value.regnum, left_value.regnum, right_value.regnum, IR_TYPE_SINT64);
                } break;

                case EXPR_OP_MODULO:

                case EXPR_OP_LESS:
                case EXPR_OP_GREATER:
                case EXPR_OP_LESS_EQUAL:
                case EXPR_OP_GREATER_EQUAL:
                case EXPR_OP_EQUAL:
                case EXPR_OP_NOT_EQUAL:

                case EXPR_OP_BITWISE_OR:
                case EXPR_OP_BITWISE_AND:
                case EXPR_OP_BITWISE_XOR:
                case EXPR_OP_BITWISE_LSHIFT:
                case EXPR_OP_BITWISE_RSHIFT:
                case EXPR_OP_BITWISE_NEGATE:

                case EXPR_OP_LOGICAL_NOT:
                case EXPR_OP_LOGICAL_AND:
                case EXPR_OP_LOGICAL_OR:
    
                case EXPR_OP_ADDRESS_OF:
                case EXPR_OP_DEREF:
                default: ASSERT(false);
            }

            free_register(context, left_value.regnum);
            free_register(context, right_value.regnum);
        } break;
        case EXPR_ASSIGN: {
            ASTExpression_Assign* expr_assign = (ASTExpression_Assign*) _expression;

            IRValue ref = generate_reference(context, expr_assign->ref);

            if (expr_assign->value->kind == EXPR_LITERAL) {
                ASTExpression_Literal* value = (ASTExpression_Literal*) expr_assign->value;
                switch (value->literal_kind) {
                    case EXPR_LITERAL_STRING: {
                        // @TODO Check type of ref. Is it pointer to char[], pointer to string or something else?

                        int length = value->string_value.len;
                        int offset = submit_rodata_string(context, cstr(value->string_value));

                        int reg_length = allocate_register(context);
                        int reg_ptr = allocate_register(context);

                        ir_imm32(&context->builder, reg_length, length, IR_TYPE_UINT64);
                        ir_address_of_variable(&context->builder, reg_ptr, context->driver->sectionid_rodata, offset);

                        ir_store(&context->builder, ref.regnum, reg_ptr, 0, IR_TYPE_SINT64);
                        ir_store(&context->builder, ref.regnum, reg_length, 8, IR_TYPE_SINT64);
                        free_register(context, ref.regnum);
                        free_register(context, reg_length);
                        free_register(context, reg_ptr); 
                    } break;
                    case EXPR_LITERAL_INTEGER: {
                        int reg_value = allocate_register(context);
                        ir_imm32(&context->builder, reg_value, value->int_value, IR_TYPE_UINT32);
                        ir_store(&context->builder, ref.regnum, reg_value, 0, IR_TYPE_SINT32);
                        free_register(context, ref.regnum);
                        free_register(context, reg_value);
                    } break;
                    case EXPR_LITERAL_FLOAT: {
                        int reg_value = allocate_register(context);
                        float lit_value = value->float_value;
                        ir_imm32(&context->builder, reg_value, *(u32*)&lit_value, IR_TYPE_FLOAT32);
                        ir_store(&context->builder, ref.regnum, reg_value, 0, IR_TYPE_SINT32);
                        free_register(context, ref.regnum);
                        free_register(context, reg_value);
                    } break;
                    default: ASSERT(false);
                }
                // IRValue val = generate_expression(context, expr_assign->value, 0);
                // // @TODO Handle structs and what not
                // ir_store(&context->builder, ref.regnum, val.regnum, 0, IR_TYPE_SINT64);

                // free_register(context, ref.regnum);
                // free_register(context, val.regnum);
            } else ASSERT(false);

            // return no IR value
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
                operands[arg_count + i] = allocate_register(context);
            }

            IRFunction_id id = func->ir_function_id;
            
            ir_call(&context->builder, id, func->parameters.len, func->return_values.len, operands);

            for (int i=0;i<arg_count + ret_count;i++) {
                free_register(context, operands[i]);
            }

            // @TODO How to return multiple IR values?
            //    Only assignment allows multiple IR values

        } break;
        default: ASSERT(false);
    }
    PROFILE_END();
    return ir_value;
}


