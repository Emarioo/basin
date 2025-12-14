#include "basin/frontend/ast.h"




static void print_indent(int depth) {
    for (int i=0;i<depth;i++) {
        printf("  ");
    }
}

void print_ast(AST* ast) {
    printf("root: ");
    print_expression(ast->global_block, 1);
}

void print_expression(ASTExpression* _expr, int depth) {
    switch(_expr->kind) {
        case EXPR_BLOCK: {
            ASTExpression_Block* expr = (ASTExpression_Block*)_expr;
            printf("BLOCK\n");
            for (int i=0;i<expr->expressions.len;i++) {
                print_indent(depth);
                print_expression(expr->expressions.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->functions.len;i++) {
                print_indent(depth);
                print_function(expr->functions.ptr[i], depth + 1);
            }
            for (int i=0;i<expr->structs.len;i++) {
                print_indent(depth);
                print_struct(expr->structs.ptr[i], depth + 1);
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
            printf("condition:\n");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body:\n");
            print_expression(expr->body_expr, depth + 1);
        } break;
        case EXPR_WHILE: {
            ASTExpression_While* expr = (ASTExpression_While*)_expr;
            printf("WHILE\n");
            
            print_indent(depth);
            printf("condition:\n");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body:\n");
            print_expression(expr->body_expr, depth + 1);
        } break;
        case EXPR_IF: {
            ASTExpression_If* expr = (ASTExpression_If*)_expr;
            printf("IF\n");
            
            print_indent(depth);
            printf("condition:\n");
            print_expression(expr->condition_expr, depth + 1);

            print_indent(depth);
            printf("body:\n");
            print_expression(expr->body_expr, depth + 1);
            
            print_indent(depth);
            printf("else:\n");
            print_expression(expr->else_expr, depth + 1);
        } break;
        case EXPR_CALL: {
            // ASTExpression_Call* expr = (ASTExpression_Call*)_expr;
            printf("CALL\n");
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
            printf("ASSIGN\n");
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
    print_indent(depth);
    printf("FUNCTION\n");
}
void print_struct(ASTStruct* struc, int depth) {
    print_indent(depth);
    printf("STRUCT\n");
}

