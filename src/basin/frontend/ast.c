#include "basin/frontend/ast.h"



bool find_identifier(cstring name, AST* ast, ASTExpression_Block* block, FindResult* result) {
    memset(result, 0, sizeof(*result));
    
    // This is the slowest function you've ever seen.
    // We need some serious optimizations here.

    // @TODO This functions finds the first identifier.
    //    This would allow duplicates which is bad.
    //    We need to search all scopes and if we find two items
    //    with the same name return an error.
    //    It might be more efficient to perform this duplicate check
    //    in an earlier stage instead of each instance of an identifier
    //    in expression performing this check.
    
    #define FIND(B_FIELD, R_FIELD, T, K)                            \
        for (int i=0;i<block->B_FIELD.len;i++) {                    \
            T* v = block->B_FIELD.ptr[i];                          \
            if (string_equal(name, cstr(v->name))) {                \
                result->block = block;                              \
                result->R_FIELD = v;                                \
                result->kind = K;                                   \
                return true;                                        \
            }                                                       \
        }
    #define FINDP(B_FIELD, R_FIELD, T, K)                            \
        for (int i=0;i<block->B_FIELD.len;i++) {                    \
            T* v = &block->B_FIELD.ptr[i];                          \
            if (string_equal(name, cstr(v->name))) {                \
                result->block = block;                              \
                result->R_FIELD = v;                                \
                result->kind = K;                                   \
                return true;                                        \
            }                                                       \
        }

    FIND(constants, f_constant, ASTConstant, FOUND_CONSTANT)
    FIND(globals,   f_global,   ASTGlobal,   FOUND_GLOBAL)
    FIND(variables, f_variable, ASTVariable, FOUND_VARIABLE)
    FIND(structs,   f_struct,   ASTStruct,   FOUND_STRUCT)
    FIND(functions, f_function, ASTFunction, FOUND_FUNCTION)
    FINDP(libraries, f_library,  ASTLibrary,  FOUND_LIBRARY)

    for (int i=0;i<block->enums.len;i++) {
        ASTEnum* v = block->enums.ptr[i];
        if (string_equal(name, cstr(v->name))) {
            result->block = block;
            result->f_enum = v;
            result->kind = FOUND_ENUM;
            return true;
        }
        if (v->share) {
            for (int j=0;j<block->enums.len;j++) {
                ASTEnum_Member* m = &block->enums.ptr[i]->members.ptr[j];
                
                if (string_equal(name, cstr(m->name))) {
                    result->block = block;
                    result->f_enum_member = m;
                    result->kind = FOUND_ENUM_MEMBER;
                    return true;
                }
            }
        }
    }

    #undef FIND

    if (block->parent) {
        bool res = find_identifier(name, ast, block->parent, result);
        if (res)
            return true;
    }

    for (int i=0;i<block->imports.len;i++) {
        ASTImport* v = &block->imports.ptr[i];

        if (string_equal(name, cstr(v->name))) {
            result->block = block;
            result->f_import = v;
            result->kind = FOUND_IMPORT;
            return true;
        }

        // Driver, task system has a bug if AST isn't available.
        // lex_and_parse needs to run for all imports that find_identifier
        // can reach on the provided ast and block.
        ASSERT(v->import->ast);

        bool res = find_identifier(name, v->import->ast, v->import->ast->global_block, result);
        if (res)
            return true;
    }

    return false;
}

int find_function_parameter(cstring name, ASTFunction* func) {
    for(int i=0;i<func->parameters.len;i++) {
        ASTFunction_Parameter* param = &func->parameters.ptr[i];
        if (string_equal(cstr(param->name), name)) {
            return i;
        }
    }
    return -1;
}

static void print_indent(int depth) {
    for (int i=0;i<depth;i++) {
        printf("  ");
    }
}

void print_ast(AST* ast) {
    printf("root: ");
    print_expression((ASTExpression*)ast->global_block, 1);
}

void print_expression(ASTExpression* _expr, int depth) {
    switch(_expr->kind) {
        case EXPR_BLOCK: {
            ASTExpression_Block* expr = (ASTExpression_Block*)_expr;
            printf("BLOCK\n");
            for (int i=0;i<expr->imports.len;i++) {
                print_indent(depth);
                print_import(&expr->imports.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->functions.len;i++) {
                print_indent(depth);
                print_function(expr->functions.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->structs.len;i++) {
                print_indent(depth);
                print_struct(expr->structs.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->enums.len;i++) {
                print_indent(depth);
                print_enum(expr->enums.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->globals.len;i++) {
                print_indent(depth);
                print_global(expr->globals.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->constants.len;i++) {
                print_indent(depth);
                print_constant(expr->constants.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->variables.len;i++) {
                print_indent(depth);
                print_variable(expr->variables.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->expressions.len;i++) {
                print_indent(depth);
                print_expression(expr->expressions.ptr[i], depth + 1);
            }
        } break;
        case EXPR_FOR: {
            ASTExpression_For* expr = (ASTExpression_For*)_expr;
            printf("FOR\n");
            
            print_indent(depth);
            printf("item: %s\n", expr->item_name.ptr);
            print_indent(depth);
            printf("index: %s\n", expr->index_name.ptr);

            print_indent(depth);
            printf("condition: ");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body: ");
            print_expression(expr->body_expr, depth + 1);
        } break;
        case EXPR_WHILE: {
            ASTExpression_While* expr = (ASTExpression_While*)_expr;
            printf("WHILE\n");
            
            print_indent(depth);
            printf("condition: ");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body: ");
            print_expression(expr->body_expr, depth + 1);
        } break;
        case EXPR_IF: {
            ASTExpression_If* expr = (ASTExpression_If*)_expr;
            printf("IF\n");
            
            print_indent(depth);
            printf("condition: ");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body: ");
            print_expression(expr->body_expr, depth + 1);
            
            print_indent(depth);
            printf("else: ");
            print_expression(expr->else_expr, depth + 1);
        } break;
        case EXPR_SWITCH: {
            ASTExpression_Switch* expr = (ASTExpression_Switch*)_expr;
            printf("SWITCH\n");
            
            print_indent(depth);
            printf("selector: ");
            print_expression(expr->selector, depth + 1);
            
            for (int i=0;i<expr->cases.len;i++) {
                print_indent(depth);
                if (expr->cases.ptr[i].conditions.len != 0) {
                    printf("case_%d: ", i);
                }
                for (int ic=0;ic<expr->cases.ptr[i].conditions.len;ic++) {
                    print_expression(expr->cases.ptr[i].conditions.ptr[ic], depth + 2);
                }
                if (expr->cases.ptr[i].conditions.len == 0) {
                    printf("default:\n");
                }
                print_indent(depth+1);
                if (expr->cases.ptr[i].body) {
                    print_expression(expr->cases.ptr[i].body, depth + 2);
                } else {
                    printf("empty");
                }
            }
        } break;
        case EXPR_CALL: {
            ASTExpression_Call* expr = (ASTExpression_Call*)_expr;
            printf("CALL ");
            print_expression(expr->expr, depth + 1);

            for (int i=0;i<expr->polymorphic_args.len;i++) {
                print_indent(depth);
                printf("polyarg%d: ", i);
                print_expression(expr->polymorphic_args.ptr[i].expr, depth + 1);
            }

            for (int i=0;i<expr->arguments.len;i++) {
                print_indent(depth);

                if (expr->arguments.ptr[i].name.ptr)
                    printf("%s: ", expr->arguments.ptr[i].name.ptr);
                else
                    printf("arg%d: ", i);
                print_expression(expr->arguments.ptr[i].expr, depth + 1);
            }
        } break;
        case EXPR_RETURN: {
            ASTExpression_Return* expr = (ASTExpression_Return*)_expr;
            printf("RETURN\n");

            for (int i=0;i<expr->exprs.len;i++) {
                print_indent(depth);
                print_expression(expr->exprs.ptr[i], depth + 1);
            }
        } break;
        case EXPR_YIELD: {
            ASTExpression_Yield* expr = (ASTExpression_Yield*)_expr;
            printf("YIELD\n");

            for (int i=0;i<expr->exprs.len;i++) {
                print_indent(depth);
                print_expression(expr->exprs.ptr[i], depth + 1);
            }
        } break;
        case EXPR_CONTINUE: {
            printf("CONTINUE\n");
        } break;
        case EXPR_BREAK: {
            printf("BREAK\n");
        } break;
        case EXPR_ASSEMBLY: {
            printf("ASSEMBLY\n");
        } break;
        case EXPR_ASSIGN: {
            ASTExpression_Assign* expr = (ASTExpression_Assign*)_expr;
            printf("ASSIGN\n");
            
            print_indent(depth);
            print_expression(expr->ref, depth + 1);
            
            print_indent(depth);
            print_expression(expr->value, depth + 1);
        } break;
        case EXPR_MEMBER: {
            ASTExpression_Member* expr = (ASTExpression_Member*)_expr;
            printf("MEMBER %s\n", expr->name.ptr);
            
            print_indent(depth);
            print_expression(expr->expr, depth + 1);
        } break;
        case EXPR_IDENTIFIER: {
            ASTExpression_Identifier* expr = (ASTExpression_Identifier*)_expr;
            printf("IDENTIFIER %s\n", expr->name.ptr);
        } break;
        case EXPR_INITIALIZER: {
            ASTExpression_Initializer* expr = (ASTExpression_Initializer*)_expr;
            printf("INITIALIZER\n");

            for (int i=0;i<expr->elements.len;i++) {
                print_indent(depth);
                if (expr->elements.ptr[i].name.len)
                    printf("%s: ", expr->elements.ptr[i].name.ptr);
                print_expression(expr->elements.ptr[i].expr, depth + 1);
            }
        } break;
        case EXPR_LITERAL: {
            ASTExpression_Literal* expr = (ASTExpression_Literal*)_expr;
            switch (expr->literal_kind) {
                case EXPR_LITERAL_INTEGER:
                    printf("LITERAL %lld\n", (long long int)expr->int_value);
                    break;
                case EXPR_LITERAL_FLOAT:
                    printf("LITERAL %lf\n", expr->float_value);
                    break;
                case EXPR_LITERAL_STRING:
                    printf("LITERAL \"%s\"\n", expr->string_value.ptr);
                    break;
                default: fprintf(stderr, "print literal, missing literal kind %d\n", expr->literal_kind); ASSERT(false);
            }
        } break;
        case EXPR_UNARY: {
            ASTExpression_Unary* expr = (ASTExpression_Unary*)_expr;
            switch (expr->op_kind) {
                       case EXPR_OP_ADDRESS_OF:     printf("ADDRESS_OF\n");
                break; case EXPR_OP_DEREF:          printf("DEREF\n");
                break; case EXPR_OP_BITWISE_NEGATE: printf("BITWISE_NEGATE\n");
                break; case EXPR_OP_LOGICAL_NOT:    printf("LOGICAL_NOT\n");
                break; case EXPR_OP_SUB:            printf("SUB\n");
                break; default: fprintf(stderr, "print unary, missing op kind %d\n", expr->op_kind); ASSERT(false);
            }
            print_indent(depth);
            print_expression(expr->expr, depth + 1);
        } break;
        case EXPR_BINARY: {
            ASTExpression_Binary* expr = (ASTExpression_Binary*)_expr;
            switch (expr->op_kind) {
                       case EXPR_OP_ADD:            printf("ADD\n");
                break; case EXPR_OP_SUB:            printf("SUB\n");
                break; case EXPR_OP_MUL:            printf("MUL\n");
                break; case EXPR_OP_DIV:            printf("DIV\n");
                break; case EXPR_OP_MODULO:         printf("MODULO\n");
                break; case EXPR_OP_LESS:           printf("LESS\n");
                break; case EXPR_OP_GREATER:        printf("GREATER\n");
                break; case EXPR_OP_LESS_EQUAL:     printf("LESS_EQUAL\n");
                break; case EXPR_OP_GREATER_EQUAL:  printf("GREATER_EQUAL\n");
                break; case EXPR_OP_EQUAL:          printf("EQUAL\n");
                break; case EXPR_OP_NOT_EQUAL:      printf("NOT_EQUAL\n");
                break; case EXPR_OP_BITWISE_OR:     printf("BITWISE_OR\n");
                break; case EXPR_OP_BITWISE_AND:    printf("BITWISE_AND\n");
                break; case EXPR_OP_BITWISE_XOR:    printf("BITWISE_XOR\n");
                break; case EXPR_OP_BITWISE_LSHIFT: printf("BITWISE_LSHIFT\n");
                break; case EXPR_OP_BITWISE_RSHIFT: printf("BITWISE_RSHIFT\n");
                break; case EXPR_OP_LOGICAL_AND:    printf("LOGICAL_AND\n");
                break; case EXPR_OP_LOGICAL_OR:     printf("LOGICAL_OR\n");
                break; default: fprintf(stderr, "print unary, missing op kind %d\n", expr->op_kind); ASSERT(false);
            }
            print_indent(depth);
            print_expression(expr->left, depth + 1);
            print_indent(depth);
            print_expression(expr->right, depth + 1);
        } break;
        default: fprintf(stderr, "print_expression, missing expr kind %d\n", _expr->kind); ASSERT(false);
    }
}
void print_function(ASTFunction* func, int depth) {
    printf("FUNCTION %s\n", func->name.ptr);

    // for (int i=0;i<func->parameters.len;i++) {
    //     print_indent(depth);
    //     print_struct(expr->structs.ptr[i], depth + 1);
    // }
    // for (int i=0;i<func->return_values.len;i++) {
    //     print_indent(depth);
    //     print_struct(expr->structs.ptr[i], depth + 1);
    // }

    if (func->body) {
        // no body means external function
        print_indent(depth);
        print_expression(func->body, depth + 1);
    }
}
void print_struct(ASTStruct* struc, int depth) {
    printf("STRUCT %s\n", struc->name.ptr);
}
void print_enum(ASTEnum* enu, int depth) {
    printf("ENUM %s : %s\n", enu->name.ptr, enu->type_name.ptr);
}
void print_global(ASTGlobal* object, int depth) {
    printf("GLOBAL %s : %s\n", object->name.ptr, object->type_name.ptr);
    if (object->value)
        print_expression(object->value, depth + 1);
}
void print_constant(ASTConstant* object, int depth) {
    printf("CONST %s : %s\n", object->name.ptr, object->type_name.ptr);
    print_expression(object->value, depth + 1);
}
void print_variable(ASTVariable* object, int depth) {
    printf("VARIABLE %s : %s\n", object->name.ptr, object->type_name.ptr);
}
void print_import(ASTImport* imp, int depth) {
    printf("IMPORT %s (shared: %d)\n", imp->name.ptr, (int)imp->shared);
}


