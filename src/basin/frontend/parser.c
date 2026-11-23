/*
    The parsing and matching pretty much always uses TokenExt since it encompasses
    Token and TokenExt.
*/

#include "basin/frontend/parser.h"

#include "basin/core/driver.h"
#include "basin/frontend/ast.h"

#include "platform/platform.h"
#include "basin/common.h"

#include <setjmp.h>
#include <stdarg.h>

#define K_STRUCT   "struct"
#define K_FN       "fn"
#define K_ENUM     "enum"
#define K_GLOBAL   "global"
#define K_IMPORT   "import"
#define K_LIBRARY  "library"
#define K_AS       "as"
#define K_FROM     "from"
#define K_CONST    "const"
#define K_VAR      "var"
#define K_FOR      "for"
#define K_WHILE    "while"
#define K_IF       "if"
#define K_ELSE     "else"
#define K_SWITCH   "switch"
#define K_CASE     "case"
#define K_DEFAULT  "default"
#define K_IN       "in"
#define K_RETURN   "return"
#define K_YIELD    "yield"
#define K_CONTINUE "continue"
#define K_BREAK    "break"
#define K_DEFER    "defer"

#define EOF_RUNE 0
typedef u32 Rune;


#define match_rune(RUNE) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_rune(context, RUNE, false))
#define match_word(WORD) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_word(context, WORD, false))
#define match_string() (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_string(context, &context->temp_string, false), context->temp_string)
#define match_int(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_int(context, PTR, false))
#define match_float(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_float(context, PTR, false))
#define match_id() (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_id(context, &context->temp_cstring, false), context->temp_cstring)
#define try_match_rune(RUNE) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_rune(context, RUNE, true))
#define try_match_id(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_id(context, PTR, true))
#define try_match_int(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_int(context, PTR, true))
#define try_match_float(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_float(context, PTR, true))
#define try_match_string(PTR) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_string(context, PTR, true))
#define try_match_word(WORD) (  context->c_location.path = __FILE__, context->c_location.line = __LINE__, _match_word(context, WORD, true))
#define peek_rune(N) _peek_rune(context, N)
#define advance() _advance(context)
#define srcloc() _srcloc(context)
#define skip_space() _skip_space(context)
#define headpos() _headpos(context)
#define restorepos(POS) context->head = POS;


#ifdef TRACY_ENABLED
    #define PROFILE_START() TracyCZone(zone, 1); push_profile_zone(context, zone)
    #define PROFILE_END() TracyCZoneEnd(zone); pop_profile_zone(context)
#else
    #define PROFILE_START()
    #define PROFILE_END()
#endif

typedef enum {
    COMPTIME_NONE,
    COMPTIME_NORMAL,
    COMPTIME_EVAL,
    COMPTIME_EMIT,
} ComptimeKind;

typedef struct {
    Compilation* compilation;
    Driver* driver;
    Import* import;
    u32 head; // token head
    
    ASTExpression_Block* previous_block;
    ASTFunction*         current_function;

    ComptimeKind comptime_kind;
    
    cstring temp_cstring;
    string temp_string;

    SourceLocation bad_location;
    jmp_buf jump_state;
    CLocation c_location; // token causing bad syntax
    char error_message[512];

    // Zones for tracy are cleaned up when long jumping.
    int zones_cap;
    int zones_len;
    TracyCZoneCtx* zones;
} ParserContext;

static inline void cleanup_profile_zones(ParserContext* ctx) {
    while (ctx->zones_len) {
        ctx->zones_len--;
        TracyCZoneEnd(ctx->zones[ctx->zones_len]);
    }
}

static inline void push_profile_zone(ParserContext* ctx, TracyCZoneCtx zone) {
    if (ctx->zones_len + 1 >= ctx->zones_cap) {
        int new_cap = ctx->zones_cap * 2 + 100;
        void* new_zones = mem__allocate(new_cap, ctx->zones);
        ctx->zones = new_zones;
        ctx->zones_cap = new_cap;
    }
    ctx->zones[ctx->zones_len] = zone;
    ctx->zones_len++;
}
static inline void pop_profile_zone(ParserContext* ctx) {
    ASSERT(ctx->zones_len > 0);
    ctx->zones_len--;
}

#define CREATE_EXPR(V, T, KIND, LOC)      \
    T* V = HEAP_ALLOC_OBJECT(T);          \
    V->kind = KIND;                       \
    V->location = LOC;


#define SET_EXPR(V, T, KIND, LOC)         \
    V = HEAP_ALLOC_OBJECT(T);          \
    V->kind = KIND;                       \
    V->location = LOC;

    
typedef enum {
    IN_FILE_SCOPE   = 0x1,
    IN_CASE_SCOPE = 0x2,
} ParseBlockFlags;


bool parse_type(ParserContext* context, ASTType* typename);
ASTExpression* parse_expression(ParserContext* context);
ASTExpression_Block* parse_block_expression(ParserContext* context, ParseBlockFlags block_flags);
ASTFunction* parse_function(ParserContext* context);
ASTEnum* parse_enum(ParserContext* context);
ASTStruct* parse_struct(ParserContext* context);


void _parse_error(ParserContext* context, SourceLocation loc, char* fmt, ...);
static SourceLocation _srcloc(ParserContext* context);
static bool _skip_space(ParserContext* context);
static int _peek_rune(ParserContext* context, int n);
static u32 _headpos(ParserContext* context);
static bool _match_rune(ParserContext* context, int rune, bool peek_operation);
static bool _match_word(ParserContext* context, const char* word, bool peek_operation);
static bool _match_string(ParserContext* context, string* outstring, bool peek_operation);
static bool _match_int(ParserContext* context, i64* out_value, bool peek_operation);
static bool _match_float(ParserContext* context, double* out_value, bool peek_operation);
static bool _match_id(ParserContext* context, cstring* outstring, bool peek_operation);
static void _advance(ParserContext* context);


void import_init_line_positions(Import* import);
static int pos_to_line_index(Import* import, int position);


#define parse_error(TOK, FMT, ...) do { (context->c_location.path = __FILE__, context->c_location.line = __LINE__, _parse_error(context, TOK, FMT __VA_OPT__(,)  __VA_ARGS__)); } while (0)

#define check_error_ext(T, ...) do { if ((T)->kind == T_END_OF_FILE) parse_error((const TokenExt*)(void*)(T), __VA_ARGS__); } while (0)

void enter_comptime_mode(ParserContext* context, ComptimeKind kind) {
    ASSERT(context->comptime_kind == COMPTIME_NONE);
    context->comptime_kind = kind;
}
void exit_comptime_mode(ParserContext* context) {
    ASSERT(context->comptime_kind != COMPTIME_NONE);
    context->comptime_kind = COMPTIME_NONE;
}

#define IS_COMPTIME() (context->comptime_kind != COMPTIME_NONE)


Result parse_import(Compilation* compilation, Import* import, AST** out_ast) {
    TracyCZone(zone, 1);
    // We implement this recursively because it's easier to debug issues
    Result result = {};
    result.kind = SUCCESS;
    result.message.ptr = NULL;
    result.message.max = 0;
    result.message.len = 0;

    AST* ast = HEAP_ALLOC_OBJECT(AST);
    ast->import = import;

    ParserContext context = {0};
    context.import = import;
    context.compilation = compilation;
    context.driver = compilation->driver;
    context.head = 0;

    int res = setjmp(context.jump_state);

    if (res == 0) {
        string text = import->text;

        if (text.len >= 2 && text.ptr[context.head] == '#' && text.ptr[context.head+1] == '!') {
            // Skip shebang
            context.head += 2;
            while (context.head < text.len) {
                context.head++;
                if (text.ptr[context.head-1] == '\n')
                    break;
            }
        }

        ast->global_block = parse_block_expression(&context, true);

        *out_ast = ast;
    } else {
        cleanup_profile_zones(&context);

        int line, column;
        string code;
        bool yes = compute_source_info(import, context.bad_location, &line, &column, &code);

        char buffer[1024];
        int len = 0;
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[0;30m%s:%d\033[0m\n", context.c_location.path, context.c_location.line);
        len += snprintf(buffer + len, sizeof(buffer) - len, "\033[0;31m%s:%d:%d:\033[0m %s\n%s", import->path.ptr, line, column, context.error_message, code.ptr);
        result.kind = FAILURE;
        result.message = string_clone_cptr(buffer);
    }
    TracyCZoneEnd(zone);
    return result;
}

ASTExpression_Block* parse_block_expression(ParserContext* context, ParseBlockFlags block_flags) {
    PROFILE_START();

    bool in_file_scope = block_flags & IN_FILE_SCOPE;
    bool in_case_scope = block_flags & IN_CASE_SCOPE;

    skip_space();

    // const TokenExt* block_tok = peek(0);
    SourceLocation block_loc = srcloc();
    if (!in_file_scope && !in_case_scope) {
        match_rune('{');
        skip_space();
    }
    
    CREATE_EXPR(block_expr, ASTExpression_Block, EXPR_BLOCK, block_loc);
    block_expr->parent = context->previous_block;
    context->previous_block = block_expr;

    while (true) {
        skip_space();
        SourceLocation tok_loc = srcloc();
        Rune tok = peek_rune(0);

        if (tok == EOF_RUNE && in_file_scope) {
            break;
        }

        if (tok == '@') {
            advance();
            ASTAnnotation anot = {};
            
            cstring anot_name = match_id();
            anot.name = string_clone_cstr(anot_name);

            Rune tok_start = peek_rune(0);
            if (tok_start == '(') {
                advance();
                u32 tok_start_pos = headpos();
                u32 tok_end_pos = -1;
                Rune tok_end = EOF_RUNE;
                int depth = 1;
                while (true) {
                    tok_end_pos = headpos();
                    tok_end = peek_rune(0);
                    if (tok_end == EOF_RUNE) {
                        SourceLocation loc = srcloc();
                        parse_error(loc, "Sudden end of file in annotantion");
                        break;
                    } else if (tok_end == '(') {
                        depth++;
                    } else if (tok_end == ')') {
                        depth--;
                        if (depth == 0) {
                            advance();
                            break;
                        }
                    }
                    advance();
                }

                if (tok_end_pos - tok_start_pos > 0) {
                    anot.content = string_clone(context->import->text.ptr + tok_start_pos, tok_end_pos - tok_start_pos);
                }
            }

            debug("ANOT %s '%.*s'\n", anot.name.ptr, anot.content.len, anot.content.ptr);

            // TODO: Add annotations to the AST
            continue;
        }

        if(try_match_word(K_IMPORT)) {
            // advance();
            skip_space();
            
            SourceLocation path_loc = srcloc();
            string path = match_string();
            skip_space();
            
            string resolved_path = comp_resolve_import_path(context->compilation, context->import, cstr(path));
            if (resolved_path.len == 0) {
                parse_error(path_loc, "Could not resolve path '%s'. Use 'import \"linux\"' to resolve from import directories, use 'import \"./hello.bsn\"' to resolve from relative directory relative to current source file.", path.ptr);
            }
        
            // @TODO Check if import already exists.
            Task task = {};
            task.kind = TASK_PARSE;
            task.compilation = context->compilation;
            task.lex_and_parse.import = driver_create_import_id(context->driver, cstr(resolved_path));

            string_cleanup(&resolved_path);

            driver_add_task(context->driver, &task);

            ASTImport new_import = {};
            new_import.location = path_loc;
            new_import.shared = false; // set from annontation @shared
            new_import.import = task.lex_and_parse.import;
            
            // @TODO Implement annotation '@external(libc) import "unistd.h"'
            //   We don't reserve T_FROM anymore
            // tok = peek(0);
            // if (tok->kind == T_FROM) {
            //     advance();
            //     parse_error(tok, "@TODO implement 'import \"main\" from lib'");
            // }

            if (try_match_word(K_AS)) {
                skip_space();
                cstring name = match_id();
                new_import.name = string_clone_cstr(name);
            }

            array_push(&block_expr->imports, &new_import);

            // @TODO Add import to scope tree
        } else if(try_match_word(K_LIBRARY)) {
            // advance();
            skip_space();
            
            SourceLocation path_loc = srcloc();
            string path = match_string();
            skip_space();

            ASTLibrary new_library = {};
            new_library.location = path_loc;
            new_library.library_name = path;

            if (try_match_word(K_AS)) {
                skip_space();
                cstring name = match_id();
                new_library.name = string_clone_cstr(name);
            }
            
            array_push(&block_expr->libraries, &new_library);

        } else if (try_match_word(K_GLOBAL)) {
            skip_space();
            
            SourceLocation name_loc = srcloc();
            cstring name = match_id();
            skip_space();
            
            match_rune(':');
            skip_space();
            
            ASTGlobal* data_object = HEAP_ALLOC_OBJECT(ASTGlobal);
            data_object->location = name_loc;
            data_object->name = string_clone_cstr(name);
            

            bool res = parse_type(context, &data_object->type_name);
            ASSERT(res);
            
            Rune equal_tok = peek_rune(0);
            if (equal_tok == '=') {
                advance();
                skip_space();
                data_object->value = parse_expression(context);
            }

            array_push(&block_expr->globals, &data_object);
        } else if (try_match_word(K_CONST)) {
            skip_space();
            
            SourceLocation name_loc = srcloc();
            cstring name = match_id();
            skip_space();
            
            ASTConstant* data_object = HEAP_ALLOC_OBJECT(ASTConstant);
            data_object->location = name_loc;
            data_object->name = string_clone_cstr(name);

            Rune colon_tok = peek_rune(0);
            if (colon_tok == ':') {
                advance();
                skip_space();
                bool res = parse_type(context, &data_object->type_name);
                ASSERT(res);
            }
            
            match_rune('=');
            skip_space();
            data_object->value = parse_expression(context);
            
            array_push(&block_expr->constants, &data_object);
        } else if (try_match_word(K_ENUM)) {
            skip_space();
            
            ASTEnum* enu = parse_enum(context);
            array_push(&block_expr->enums, &enu);

        } else if (try_match_word(K_STRUCT)) {
            skip_space();
            
            ASTStruct* struc = parse_struct(context);
            array_push(&block_expr->structs, &struc);

        } else if (try_match_word(K_FN)) {
            skip_space();
            
            ASTFunction* function = parse_function(context);
            array_push(&block_expr->functions, &function);

        } else if (try_match_rune('}')) {
            if (!in_file_scope) {
                break;
            }
            parse_error(tok_loc, "Unexpected closing brace. (at file scope)");
        } else if (try_match_word(K_CASE) || try_match_word(K_DEFAULT)) {
            if (in_case_scope) {
                break;
            }
            parse_error(tok_loc, "'case' and 'default' is only allowed in switch scopes.");
        } else {
            u32 saved_pos = headpos();

            cstring name;
            bool matched_id = try_match_id(&name);
            skip_space();
            if (matched_id && peek_rune(0) == ':') {
                advance(); // skip :
                skip_space();
                
                ASTVariable* data_object = HEAP_ALLOC_OBJECT(ASTVariable);
                data_object->location = tok_loc;
                data_object->name = string_clone_cstr(name);

                Rune equal_tok = peek_rune(0);
                if (equal_tok != '=') {
                    SourceLocation equal_tok_loc = srcloc();
                    bool res = parse_type(context, &data_object->type_name);
                    if (!res) {
                        // @TODO The parse type function should throw error. No need for this?
                        parse_error(equal_tok_loc, "Type is bad.");
                    }
                    skip_space();
                }
                
                equal_tok = peek_rune(0);
                SourceLocation equal_tok_loc = srcloc();
                if (equal_tok == '=') {
                    advance();
                    skip_space();

                    ASTExpression* rvalue = parse_expression(context);

                    CREATE_EXPR(expr_lval, ASTExpression_Identifier, EXPR_IDENTIFIER, tok_loc);
                    expr_lval->name = string_clone_cstr(name);

                    CREATE_EXPR(expr_assign, ASTExpression_Assign, EXPR_ASSIGN, equal_tok_loc);
                    expr_assign->ref = (ASTExpression*) expr_lval;
                    expr_assign->value = rvalue;

                    array_push(&block_expr->expressions, (ASTExpression**)&expr_assign);
                }

                array_push(&block_expr->variables, &data_object);
            } else {
                restorepos(saved_pos);

                ASTExpression* expr = parse_expression(context);
                skip_space();
                
                SourceLocation equal_tok_loc = srcloc();
                Rune equal_tok = peek_rune(0);
                if (equal_tok == '=') {
                    advance();
                    skip_space();
                    ASTExpression* rvalue = parse_expression(context);

                    CREATE_EXPR(expr_assign, ASTExpression_Assign, EXPR_ASSIGN, equal_tok_loc);
                    expr_assign->ref = expr;
                    expr_assign->value = rvalue;

                    array_push(&block_expr->expressions, (ASTExpression**)&expr_assign);
                } else {
                    array_push(&block_expr->expressions, &expr);
                }
            }
        }
    }

    if (!in_file_scope && !in_case_scope) {
        skip_space();
        // match_rune('}');
    }

    context->previous_block = block_expr->parent;
    
    PROFILE_END();
    return block_expr;
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
        default: break;
    }
    fprintf(stderr, "Missing operator precedence: %d (operator enum)\n", op_kind);
    ASSERT(false);
    return 0;
}

ASTExpression* parse_expression(ParserContext* context) {
    PROFILE_START();

    // const TokenExt* tok = peek(0);

    // TODO: Parse annotations

    ASTExpression* ret_expr;

    skip_space();

    SourceLocation tok_loc = srcloc();
    if (try_match_word(K_RETURN)) {
        skip_space();

        ASTExpression_Return* out_expr = HEAP_ALLOC_OBJECT(ASTExpression_Return);
        out_expr->kind                 = EXPR_RETURN;
        out_expr->location             = tok_loc;

        SourceLocation tok0_loc = srcloc();
        Rune tok0 = peek_rune(0);
        ASTExpression* expr = NULL;
        // if ((tok->flags & TF_POST_NEWLINE) || tok0->kind == ';') {
        if (tok0 == ';') {
            advance();
            // no expression
        } else {
            while (1) {
                expr = parse_expression(context);
                if (!expr) {
                    parse_error(tok0_loc, "Expected an expression (if condition).");
                }
                skip_space();

                array_push(&out_expr->exprs, &expr);

                tok0_loc = srcloc();
                tok0 = peek_rune(0);
                if (tok0 == ',') {
                    advance();
                    continue;
                }
                break;
            }
        }

        ret_expr = (ASTExpression*)out_expr;

    } else if (try_match_word(K_YIELD)) {
        skip_space();

        ASTExpression_Yield* out_expr = HEAP_ALLOC_OBJECT(ASTExpression_Yield);
        out_expr->kind                = EXPR_YIELD;
        out_expr->location            = tok_loc;

        SourceLocation tok0_loc = srcloc();
        Rune tok0 = peek_rune(0);
        ASTExpression* expr = NULL;
        // if ((tok->flags & TF_POST_NEWLINE) || tok0->kind == ';') {
        if (tok0 == ';') {
            advance();
            // no expression
        } else {
            while (1) {
                expr = parse_expression(context);
                if (!expr) {
                    parse_error(tok0_loc, "Expected an expression (if condition).");
                }
                skip_space();

                array_push(&out_expr->exprs, &expr);

                tok0_loc = srcloc();
                tok0 = peek_rune(0);
                if (tok0 == ',') {
                    advance();
                    continue;
                }
                break;
            }
        }

        ret_expr = (ASTExpression*)out_expr;

    } else if (try_match_word(K_IF)) {
        skip_space();

        CREATE_EXPR(out_expr, ASTExpression_If, EXPR_IF, tok_loc)

        SourceLocation tok0_loc = srcloc();
        ASTExpression* expr = parse_expression(context);
        if (!expr)
            parse_error(tok0_loc, "Expected an expression (if condition).");
        skip_space();

        SourceLocation tok1_loc = srcloc();
        ASTExpression* body = parse_expression(context);
        if (!body)
            parse_error(tok1_loc, "Expected an expression (if body).");
        skip_space();

        ASTExpression* else_body = NULL;

        // const TokenExt* tok2 = peek(0);
        if (try_match_word(K_ELSE)) {
            skip_space();
            
            SourceLocation else_expr_tok_loc = srcloc();
            // const TokenExt* else_expr_tok = peek(0);
            else_body = parse_expression(context);
            if (!else_body) {
                parse_error(else_expr_tok_loc, "Expected an expression (else body).");
            }
        }
        
        out_expr->condition_expr   = expr;
        out_expr->body_expr        = body;
        out_expr->else_expr        = else_body;

        ret_expr = (ASTExpression*)out_expr;
    
    } else if (try_match_word(K_SWITCH)) {
        skip_space();
        
        SourceLocation tok0_loc = srcloc();
        // const TokenExt* tok0 = peek(0);
        ASTExpression* expr = parse_expression(context);
        if (!expr)
            parse_error(tok0_loc, "Expected an expression (if condition).");
        skip_space();
        
        match_rune('{');
        
        CREATE_EXPR(out_expr, ASTExpression_Switch, EXPR_SWITCH, tok0_loc)
        out_expr->selector = expr;

        bool has_default = false;

        while (true) {
            skip_space();
            SourceLocation tok_loc = srcloc();
            Rune tok = peek_rune(0);

            ASTExpression_Switch_Case switch_case = {};

            if (tok == '}') {
                advance();
                break;

            } else if(try_match_word(K_CASE)) {
                skip_space();
                
                while (true) {
                    ASTExpression* expr = parse_expression(context);
                    skip_space();
                    array_push(&switch_case.conditions, &expr);

                    Rune tok = peek_rune(0);
                    if (tok != ',') {
                        break;
                    }
                    advance();
                }
                skip_space();
                
                u32 saved_pos = headpos();
                Rune tok0 = peek_rune(0);
                if (tok0 == '}' || try_match_word(K_CASE) || try_match_word(K_DEFAULT)) {
                    restorepos(saved_pos);
                    // no body
                } else {
                    switch_case.body = (ASTExpression*)parse_block_expression(context, IN_CASE_SCOPE);
                }

            } else if(try_match_word(K_DEFAULT)) {
                skip_space();
                
                if (has_default) {
                    // @TODO Show location of previous default case.
                    parse_error(tok_loc, "Switch may only have one default case.");
                }
                has_default = true;
                
                u32 saved_pos = headpos();
                Rune tok0 = peek_rune(0);
                if (tok0 == '}' || try_match_word(K_CASE) || try_match_word(K_DEFAULT)) {
                    restorepos(saved_pos);
                    // no body
                } else {
                    switch_case.body = (ASTExpression*)parse_block_expression(context, IN_CASE_SCOPE);
                }
                
            } else {
                parse_error(tok_loc, "Expected '}' to close switch or 'case' to define a case and some code to run.");
            }
            array_push(&out_expr->cases, &switch_case);
        }

        ret_expr = (ASTExpression*)out_expr;

    } else if (try_match_word(K_FOR)) {
        skip_space();

        // @TODO Reverse annotation

        // const TokenExt* tok0 = peek(0);
        // const TokenExt* tok1 = peek(1);
        // const TokenExt* tok2 = peek(2);
        // const TokenExt* tok3 = peek(3);

        // for NR,IT in ITEMS BODY
        // for IT in ITEMS BODY
        // for ITEMS BODY

        string index_name = {};
        string item_name = {};

        u32 saved_pos = headpos();

        bool succcess = false;
        cstring name0;
        bool matched_id = try_match_id(&name0);
        if (matched_id) {
            skip_space();
            Rune comma_tok = peek_rune(0);
            if (comma_tok == ',') {
                advance();
                skip_space();
                cstring name1;
                matched_id = try_match_id(&name1);
                if (matched_id) {
                    skip_space();
                    if (try_match_word(K_IN)) {
                        item_name = string_clone_cstr(name0);
                        index_name = string_clone_cstr(name1);
                        succcess = true;
                    }
                }
            }
        }

        if (!succcess) {
            restorepos(saved_pos);
            
            cstring name0;
            matched_id = try_match_id(&name0);
            if (matched_id) {
                skip_space();
                if (try_match_word(K_IN)) {
                    index_name = string_clone_cptr("nr");
                    item_name = string_clone_cstr(name0);
                    succcess = true;
                }
            }
        }
        
        if (!succcess) {
            index_name = string_clone_cptr("nr");
            item_name = string_clone_cptr("it");
        }
        skip_space();

        ASTExpression* cond = parse_expression(context);
        skip_space();
        
        ASTExpression* body = parse_expression(context);

        CREATE_EXPR(expr, ASTExpression_For, EXPR_FOR, tok_loc);
        expr->index_name        = index_name;
        expr->item_name         = item_name;
        expr->condition_expr    = cond;
        expr->body_expr         = body;

        ret_expr = (ASTExpression*)expr;

    } else if (try_match_word(K_WHILE)) {
        skip_space();
        
        ASTExpression* cond = NULL;
        
        Rune tok0 = peek_rune(0);
        if (tok0 == '{') {
            // infinite loop, no condition
        } else {
            cond = parse_expression(context);
        }
        skip_space();

        ASTExpression* body = parse_expression(context);

        CREATE_EXPR(expr, ASTExpression_While, EXPR_WHILE, tok_loc);
        expr->condition_expr   = cond;
        expr->body_expr        = body;
        
        ret_expr = (ASTExpression*)expr;

    // } else if (tok->kind == '#') {
    //     // compile time expression
    //     advance();

    //     if(IS_COMPTIME()) {
    //         parse_error(tok, "Nesting compile time expressions is not allowed.");
    //     }

    //     ComptimeKind compttime_kind = COMPTIME_NORMAL;

    //     tok = peek(0);
    //     if (match_word(K_IDENTIFIER)) {
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
    } else if (try_match_word(K_CONTINUE)) {
        // skip_space();

        CREATE_EXPR(expr, ASTExpression_Continue, EXPR_CONTINUE, tok_loc);
        
        ret_expr = (ASTExpression*)expr;
    } else if (try_match_word(K_BREAK)) {
        // skip_space();
        
        CREATE_EXPR(expr, ASTExpression_Break, EXPR_BREAK, tok_loc);
        
        ret_expr = (ASTExpression*)expr;
    } else {
        
        Array_ASTExpressionP exprs;
        array_init(&exprs, 8);

        Array_ASTExpressionP ops;
        array_init(&ops, 8);

        bool expect_value = true;
        bool end_of_expression = false;

        while (true) {
            skip_space();
            SourceLocation tok0_loc = srcloc();
            Rune tok0 = peek_rune(0);
            Rune tok1 = peek_rune(1);

            cstring id_name;
            
            if (expect_value) {
                i64 int_value;
                double float_value;
                string str_value;
                if (try_match_id(&id_name)) {
                    // advance();

                    // cstring name = DATA_FROM_IDENTIFIER(tok0);
                    if (string_equal_cstr(id_name, "__FUNC__")) {
                        CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                        expr->literal_kind = EXPR_LITERAL_STRING;

                        if (context->current_function) {
                            expr->string_value = string_clone_cstr(cstr(context->current_function->name));
                        } else {
                            // @TODO No function means top scope.
                            //    Would empty string be better?
                            expr->string_value = string_clone_cptr("__topexpr__");
                        }

                        array_push(&exprs, (ASTExpression**)&expr);
                    } else if (string_equal_cstr(id_name, "__LINE__")) {
                        CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                        expr->literal_kind = EXPR_LITERAL_STRING;

                        // SourceLocation loc = location_from_token(tok0);
                        int line = 0;
                        bool res = compute_source_info(context->import, tok0_loc, &line, NULL, NULL);
                        ASSERT(res);
                        
                        char buffer[13];
                        snprintf(buffer, sizeof(buffer), "%d", line);
                        expr->string_value = string_clone_cptr(buffer);

                        array_push(&exprs, (ASTExpression**)&expr);
                    } else if (string_equal_cstr(id_name, "__COLUMN__")) {
                        CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                        expr->literal_kind = EXPR_LITERAL_STRING;

                        // SourceLocation loc = location_from_token(tok0);
                        int column = 0;
                        bool res = compute_source_info(context->import, tok0_loc, NULL, &column, NULL);
                        ASSERT(res);

                        char buffer[13];
                        snprintf(buffer, sizeof(buffer), "%d", column);
                        expr->string_value = string_clone_cptr(buffer);

                        array_push(&exprs, (ASTExpression**)&expr);
                    } else if (string_equal_cstr(id_name, "__FILE__")) {
                        CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                        expr->literal_kind = EXPR_LITERAL_STRING;
                        expr->string_value = string_clone_cstr(cstr(context->import->path));

                        array_push(&exprs, (ASTExpression**)&expr);
                    } else {
                        CREATE_EXPR(expr, ASTExpression_Identifier, EXPR_IDENTIFIER, tok0_loc);
                        expr->name = string_clone_cstr(id_name);
                        array_push(&exprs, (ASTExpression**)&expr);
                    }
                } else if (try_match_float(&float_value)) {
                    CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                    expr->literal_kind = EXPR_LITERAL_FLOAT;
                    expr->float_value = float_value;

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (try_match_int(&int_value)) {

                    CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                    expr->literal_kind = EXPR_LITERAL_INTEGER;
                    expr->int_value = int_value;

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (try_match_string(&str_value)) {
                    CREATE_EXPR(expr, ASTExpression_Literal, EXPR_LITERAL, tok0_loc);
                    expr->literal_kind = EXPR_LITERAL_STRING;

                    // cstring text = DATA_FROM_STRING(tok0);
                    // expr->string_value = string_clone_cstr(text);
                    expr->string_value = str_value;

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0 == '[') {
                    advance();
                    skip_space();
                    
                    CREATE_EXPR(expr, ASTExpression_Initializer, EXPR_INITIALIZER, tok0_loc);

                    while (true) {
                        Rune tok = peek_rune(0);

                        if (tok == ']') {
                            advance();
                            break;
                        }

                        ASTExpression_Initializer_Element element = {};
                        
                        u32 saved_pos = headpos();
                        cstring name;
                        bool matched_id = try_match_id(&name);
                        skip_space();
                        Rune tok1 = peek_rune(0);
                        if (matched_id && tok1 == '=') {
                            advance();
                            skip_space();
                            // advance();
                            // cstring name = DATA_FROM_IDENTIFIER(tok);
                            element.name = string_clone_cstr(name);
                        } else {
                            restorepos(saved_pos);
                        }

                        element.expr = parse_expression(context);
                        array_push(&expr->elements, &element);

                        SourceLocation tok_loc = srcloc();
                        tok = peek_rune(0);
                        if (tok == ']') {
                            continue;
                        } else if (tok == ',') {
                            advance();
                            continue;
                        } else {
                            parse_error(tok_loc, "Expected ',' or ']' to end or continue initializer.");
                        }
                    }

                    array_push(&exprs, (ASTExpression**)&expr);
                } else if (tok0 == '{') {
                    ASTExpression* expr = (ASTExpression*)parse_block_expression(context, false);

                    array_push(&exprs, &expr);
                } else if (tok0 == '(') {
                    advance();

                    ASTExpression* expr = parse_expression(context);

                    match_rune(')');

                    array_push(&exprs, &expr);
                } else if (tok0 == '-') {
                    advance();

                    CREATE_EXPR(expr, ASTExpression_Unary, EXPR_UNARY, tok0_loc);
                    expr->op_kind = EXPR_OP_SUB;

                    array_push(&ops, (ASTExpression**)&expr);
                    continue;
                } else {
                    parse_error(tok0_loc, "Not a valid token for expression (primary value)");
                }
                expect_value = false;
                continue;
            } else {
                ASTExpression_Unary* unary_op = NULL;
                // @TODO Check if token has whitespace before it.

                if (tok0 == '&') {
                // if (tok0 == '&' && !HAS_PRE_WHITESPACE(tok0)) {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0_loc);
                    unary_op->op_kind = EXPR_OP_ADDRESS_OF;
                // } else if (tok0 == '^' && !HAS_PRE_WHITESPACE(tok0)) {
                } else if (tok0 == '^') {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0_loc);
                    unary_op->op_kind = EXPR_OP_DEREF;
                } else if (tok0 == '~') {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0_loc);
                    unary_op->op_kind = EXPR_OP_BITWISE_NEGATE;
                } else if (tok0 == '!') {
                    SET_EXPR(unary_op, ASTExpression_Unary, EXPR_UNARY, tok0_loc);
                    unary_op->op_kind = EXPR_OP_LOGICAL_NOT;
                }
                if (unary_op) {
                    advance();
                    unary_op->expr = exprs.ptr[exprs.len-1];
                    exprs.ptr[exprs.len-1] = (ASTExpression*)unary_op;
                    continue;
                }

                ASTExpression_Binary* operation = NULL;
                
                if (tok0 == '+') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_ADD;
                } else if (tok0 == '-') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_SUB;
                } else if (tok0 == '*') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_MUL;
                } else if (tok0 == '/') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_DIV;
                } else if (tok0 == '%') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_MODULO;
                } else if (tok0 == '&' && tok1 == '&') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_LOGICAL_AND;
                } else if (tok0 == '|' && tok1 == '|') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_LOGICAL_OR;
                } else if (tok0 == '=' && tok1 == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_EQUAL;
                } else if (tok0 == '!' && tok1 == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_NOT_EQUAL;
                } else if (tok0 == '<' && tok1 == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_LESS_EQUAL;
                } else if (tok0 == '>' && tok1 == '=') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_GREATER_EQUAL;
                } else if (tok0 == '<' && tok1 == '<') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_BITWISE_LSHIFT;
                } else if (tok0 == '>' && tok1 == '>') {
                    advance();
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_BITWISE_RSHIFT;
                } else if (tok0 == '<') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_LESS;
                } else if (tok0 == '>') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_GREATER;
                } else if (tok0 == '^') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_BITWISE_XOR;
                } else if (tok0 == '&') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_BITWISE_AND;
                } else if (tok0 == '|') {
                    SET_EXPR(operation, ASTExpression_Binary, EXPR_BINARY, tok0_loc);
                    operation->op_kind = EXPR_OP_BITWISE_OR;
                } else if (tok0 == '.') {
                    advance();
                    skip_space();
                    cstring name = match_id();

                    CREATE_EXPR(expr, ASTExpression_Member, EXPR_MEMBER, tok0_loc);
                    expr->name = string_clone_cstr(name);

                    ASTExpression* last_expr = array_last(&exprs);
                    expr->expr = last_expr;
                    exprs.ptr[exprs.len-1] = (ASTExpression*)expr;
                    continue;
                } else if (tok0 == ':') {
                    advance();
                    skip_space();
                    CREATE_EXPR(expr, ASTExpression_Cast, EXPR_CAST, tok0_loc);
                    
                    // @TODO Provide good error message.
                    //   "ERROR: Expected type" is terrible.
                    //   "ERROR: Expected type (detected syntax was <expr> : <type> which is a type cast)" would be better.
                    bool res = parse_type(context, &expr->type_name);
                    ASSERT(res);

                    ASTExpression* last_expr = array_last(&exprs);
                    expr->expr = last_expr;
                    exprs.ptr[exprs.len-1] = (ASTExpression*)expr;
                    continue;
                } else {
                    Array_ASTExpression_Call_PolyArgument polyargs = {};
                    
                    bool parsed_poly_args = false;
                    // @TODO Check white for polymorphic functions?
                    if (tok0 == '<') {
                    // if (tok0 == '<' && !HAS_PRE_WHITESPACE(tok0)) {
                        advance();
                        parsed_poly_args = true;
                        
                        while (true) {
                            skip_space();
                            Rune tok = peek_rune(0);
                            if (tok == '>') {
                                advance();
                                break;
                            }
                            ASTExpression_Call_PolyArgument arg = {};
                            arg.expr = parse_expression(context);
                            array_push(&polyargs, &arg);
                            skip_space();

                            SourceLocation tok_loc = srcloc();
                            tok = peek_rune(0);

                            if (tok == '>') {
                                continue;
                            } else if (tok == ',') {
                                advance();
                                continue;
                            } else {
                                parse_error(tok_loc, "Expected '>' or ',' to continue or end polymorphic arguments.");
                            }
                        }
                    }
                    skip_space();

                    if (tok0 == '(' || parsed_poly_args) {
                        match_rune('(');
                        
                        CREATE_EXPR(expr, ASTExpression_Call, EXPR_CALL, tok0_loc);
                        expr->polymorphic_args = polyargs;
                        
                        while (true) {
                            skip_space();
                            SourceLocation tok_loc = srcloc();
                            Rune tok = peek_rune(0);
                            if (tok == ')') {
                                advance();
                                break;
                            }
                            
                            ASTExpression_Call_Argument arg = {};
                            arg.location = tok_loc;


                            u32 saved_pos = headpos();
                            SourceLocation name_loc = srcloc();
                            cstring name;
                            bool matched_id = try_match_id(&name);
                            skip_space();
                            Rune tok1 = peek_rune(1);

                            if (name.len > 0 && tok1 == '=') {
                                advance();
                                skip_space();
                                arg.name = string_clone_cstr(name);
                                arg.location = name_loc;
                            } else {
                                restorepos(saved_pos);
                            }

                            arg.expr = parse_expression(context);
                            array_push(&expr->arguments, &arg);
                            skip_space();

                            tok_loc = srcloc();
                            tok = peek_rune(0);

                            if (tok == ')') {
                                continue;
                            } else if (tok == ',') {
                                advance();
                                continue;
                            } else {
                                parse_error(tok_loc, "Expected ')' or ',' to continue or end function arguments.");
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

        ret_expr = exprs.ptr[0];
    }
    PROFILE_END();
    return ret_expr;
}


ASTFunction* parse_function(ParserContext* context) {
    PROFILE_START();

    // match_word(K_FN);

    SourceLocation name_loc = srcloc();
    cstring name = match_id();
    // check_error_ext(tok, "Expected an identifier.");
    
    skip_space();

    ASTFunction* out_function = HEAP_ALLOC_OBJECT(ASTFunction);
    out_function->name      = string_clone_cstr(name);
    out_function->location  = name_loc;

    match_rune('(');

    ASTFunction* prev_func = context->current_function;
    context->current_function = out_function;

    // Parse arguments
    while (true) {
        skip_space();
        // const TokenExt* tok_ident = peek(0);
        if (try_match_rune(')')) {
            // advance();
            break;
        }

        SourceLocation field_name_loc = srcloc();
        cstring field_name;
        bool matched_id = try_match_id(&field_name);
        if (matched_id) {
            skip_space();
            match_rune(':');
            skip_space();

            ASTType type_name = {};
            bool res = parse_type(context, &type_name);
            ASSERT(res);
            skip_space();

            Rune tok = peek_rune(0);
            if (tok == ';') {
                advance();
            }
            skip_space();
            ASTFunction_Parameter parameter;
            // cstring field_name      = DATA_FROM_IDENTIFIER(tok_ident);
            parameter.name          = string_clone_cstr(field_name);
            parameter.location      = field_name_loc;
            parameter.default_value = NULL;
            parameter.type_name     = type_name;

            array_push(&out_function->parameters, &parameter);
        } else {
            parse_error(field_name_loc, "Expected a parameter, 'item : i32;'");
        }

        Rune tok = peek_rune(0);
        if (tok == ',') {
            advance();
            continue;
        } else if(tok == ')') {
            continue;
        } else {
            SourceLocation tok_loc = srcloc();
            parse_error(tok_loc, "Expected a ')' or ','");
        }
    }
    skip_space();

    // Parse return values
    Rune tok0 = peek_rune(0);
    Rune tok1 = peek_rune(1);
    if (tok0 == '-' && tok1 == '>') {
        advance();
        advance();
        while (true) {
            skip_space();
            SourceLocation tok0_loc = srcloc();
            // const TokenExt* tok0 = peek(0);
            // const TokenExt* tok1 = peek(1);

            ASTFunction_Parameter parameter = {};
            parameter.location = tok0_loc;

            u32 saved_pos = headpos();

            cstring field_name;
            bool matched_id = try_match_id(&field_name);
            skip_space();
            if (matched_id && try_match_rune(':')) {
                // cstring field_name     = DATA_FROM_IDENTIFIER(tok0);
                parameter.name         = string_clone_cstr(field_name);
                skip_space();
            } else {
                restorepos(saved_pos);
            }
            
            bool res = parse_type(context, &parameter.type_name);
            ASSERT(res);
            skip_space();

            array_push(&out_function->return_values, &parameter);
            
            Rune tok = peek_rune(0);
            if (tok == ';') {
                advance();
                break;
            } else if (tok == '{') {
                break;
            } else if (tok == ',') {
                advance();
                continue;
            } else {
                /* @TODO It is questionable whether we allow an "implicit" semicolon
                     to end an external function declaration.

                    This syntax error:

                        @external(glfw) context(fd: i32) -> i32,
                        
                        error(23)

                    May be confused with this where we return i32 and error type
                    and on the next line we have (23) which is valid expression but
                    it does nothing, this is where user expects the function error(23) to
                    be called but isn't because they accidently put a comma on the declaration.

                        @external(glfw) context(fd: i32) -> i32, error
                        
                        (23) // does nothing, not an allowed expression

                    In this case it's likely fine, we may not allow expression that does nothing
                    to detect such syntax mistakes.
                    The error identifier is either a type or a function, it can't be both and therefore we will get an error either "ERROR: 'error' is not a valid type (return values)" or "ERROR: 'error' in 'error(23)' is not a function."

                    I want to experiment with a clean language with few semi-colons, commas.
                    Perhaps it's a bad idea but that's exactly what i want to find out for myself.

                */
                // parse_error(tok, "Expected a return value, '-> i32, i32'");
                break;
            }
        }
    }
    skip_space();

    Rune tok_body = peek_rune(0);
    if (tok_body == '{') {
        ASTExpression* body = (ASTExpression*)parse_block_expression(context, false);
        out_function->body = body;
    }
    
    IRFunction empty_func = {};

    IRFunction_id func_id = atomic_array_push(&context->compilation->program->functions, &empty_func);
    IRFunction* ir_func = atomic_array_getptr(&context->compilation->program->functions, func_id);

    ir_func->id = func_id;
    ir_func->name = string_clone_cstr(cstr(out_function->name));
    out_function->ir_function_id = func_id;

    context->current_function = prev_func;
    
    PROFILE_END();
    return out_function;
}

ASTEnum* parse_enum(ParserContext* context) {
    PROFILE_START();
    // match_word(T_ENUM);

    // advance();

    SourceLocation name_loc = srcloc();
    cstring name = match_id();
    skip_space();
    // check_error_ext(tok, "Expected an identifier.");
    
    // cstring name = DATA_FROM_IDENTIFIER(tok);
    
    ASTEnum* out_enum = HEAP_ALLOC_OBJECT(ASTEnum);
    out_enum->name      = string_clone_cstr(name);
    out_enum->location  = name_loc;

    Rune tok = peek_rune(0);
    if (tok == ':') {
        advance();
        skip_space();
        bool res = parse_type(context, &out_enum->type_name);
        ASSERT(res);
        skip_space();
    }

    match_rune('{');

    while (true) {
        skip_space();
        SourceLocation tok_loc = srcloc();
        Rune tok = peek_rune(0);

        cstring member_name;
        bool matched_id = try_match_id(&member_name);
        if (matched_id) {
            skip_space();

            ASTEnum_Member member = {};

            member.name          = string_clone_cstr(member_name);
            member.location      = tok_loc;
            
            Rune tok = peek_rune(0);
            if (tok == '=') {
                advance();
                skip_space();
                member.default_value = parse_expression(context);
                skip_space();
            }

            tok = peek_rune(0);
            if (tok == ',') {
                advance();
            }

            array_push(&out_enum->members, &member);
        } else if (tok == '}') {
            advance();
            break;
        } else {
            parse_error(tok_loc, "Expected a field, 'item = 5,'");
        }
    }
    PROFILE_END();
    return out_enum;
}


ASTStruct* parse_struct(ParserContext* context) {
    PROFILE_START();
    // match(T_STRUCT);

    SourceLocation name_loc = srcloc();
    cstring name = match_id();
    skip_space();

    ASTStruct* out_struct = HEAP_ALLOC_OBJECT(ASTStruct);
    out_struct->name      = string_clone_cstr(name);
    out_struct->location  = name_loc;

    match_rune('{');

    while (true) {
        skip_space();
        // const TokenExt* tok = peek(0);

        SourceLocation tok_loc = srcloc();
        Rune tok = peek_rune(0);

        cstring field_name;
        bool matched_id = try_match_id(&field_name);

        if (matched_id) {
            // advance();
            skip_space();
            match_rune(':');
            skip_space();

            ASTStruct_Field field = {};
            // cstring field_name = DATA_FROM_IDENTIFIER(tok);
            field.name         = string_clone_cstr(field_name);
            field.location     = tok_loc;
            parse_type(context, &field.type_name);
            
            skip_space();

            array_push(&out_struct->fields, &field);

            Rune tok = peek_rune(0);
            if (tok == ';') {
                advance();
            }

        } else if (tok == '}') {
            advance();
            break;
        } else {
            parse_error(tok_loc, "Expected a field, 'item : i32;'");
        }
        // @TODO Parse function
    }
    PROFILE_END();
    return out_struct;
}


bool parse_type(ParserContext* context, ASTType* typename) {
    PROFILE_START();
    string acc = {};
    // @TODO Disallow 'text: char*int'
    while (true) {
        // skip_space();
        SourceLocation tok_loc = srcloc();
        Rune tok = peek_rune(0);

        cstring type_name;
        bool matched_id = try_match_id(&type_name);
        if (matched_id) {
            // advance();
            // cstring str = DATA_FROM_IDENTIFIER(tok);
            string_append_cstr(&acc, type_name);
        } else if (tok == '*') {
            advance();
            string_append_char(&acc, '*');
        } else if (tok == '[') {
            Rune tok1 = peek_rune(1);
            if (tok1 == ']') {
                advance();
                advance();
                string_append_cstr(&acc, cstr_cptr("[]"));
            }
            // @TODO not complete here.
        } else {
            if (acc.len == 0)
                parse_error(tok_loc, "Invalid type.");
            break;
        }
    }
    *typename = acc;
    PROFILE_END();
    return true;
}




void import_init_line_positions(Import* import) {
    u32 head = 0;

    array_init(&import->line_positions, import->text.len / 40);

    array_push(&import->line_positions, &head);
    
    while(head < import->text.len) {
        int cur_head = head;
        char chr = import->text.ptr[head];
        head++;

        if(chr == '\n') {
            array_push(&import->line_positions, &head);
        }
    }
}

static int pos_to_line_index(Import* import, int position) {
    TracyCZone(zone, 1);
    int* data = import->line_positions.ptr;
    int  len  = import->line_positions.len;
    int  high = len-1;
    int  low  = 0;
    int  mid, val;

    while (true) {
        mid = (high+low)/2;
        val = data[mid];
        if (position < val) {
            high = mid-1;
            if (position >= data[mid-1]) {
                mid = mid-1;
                break;
            }
        } else if(position > val) {
            low = mid+1;
            if (position < data[mid+1]) {
                break;
            }
        } else if (high == low || position == val) {
            break;
        }
    }
    TracyCZoneEnd(zone);
    return mid;
}

bool compute_source_info(Import* import, SourceLocation location, int* out_line, int* out_column, string* out_code) {
    TracyCZone(zone, 1);

    bool result = false;

    if(out_line) {
        *out_line = 0;
    }
    if(out_column) {
        *out_column = 0;
    }
    if(out_code) {
        out_code->ptr = NULL;
        out_code->len = 0;
        out_code->max = 0;
    }
    if(location.import_id == INVALID_IMPORT_ID) {
        goto end;
    }
    
    string text = {};
    text.ptr = import->text.ptr;
    text.len = import->text.len;

    if(!text.ptr) {
        goto end;
    }

    int nr_tabs = 0;
    int line_index = pos_to_line_index(import, location.position);
    int line = line_index + 1;
    int column = 1;
    int head_at_line_start = import->line_positions.ptr[line_index];
    u32 head = head_at_line_start;

    while(head < text.len) {
        char chr = text.ptr[head];
        head++;

        if(chr == '\n') {
            head--;
            break;
        }
        if(head <= location.position) {
            column+=1;
            if(chr == '\t') {
                nr_tabs++;
            }
        }
    }
    
    if(out_line) {
        *out_line = line;
    }
    if(out_column) {
        *out_column = column;
    }
    if(out_code) {
        char* code = text.ptr + head_at_line_start;
        int len = head - head_at_line_start;
        // Create code string like this:
        //     value = call_func(1, x, 5)
        //             ^

        out_code->len = len + 1 // length of characters on the line + newline
            + (column-1) + 1 + 1; // amount of spaces + arrow + newline
        out_code->max = out_code->len;
        out_code->ptr = mem__alloc(out_code->max + 1); // +1 for null character
        memcpy(out_code->ptr, code, len);
        out_code->ptr[len] = '\n';
        out_code->ptr[out_code->len] = '\0';

        char* arrow_code = out_code->ptr + len + 1;

        int arrow_head = 0;
        while(arrow_head < nr_tabs) {
            arrow_code[arrow_head++] = '\t';
        }
        while(arrow_head < column-1) {
            arrow_code[arrow_head++] = ' ';
        }
        arrow_code[arrow_head] = '^';
        arrow_code[arrow_head+1] = '\n';
    }

    result = true;

end:
    TracyCZoneEnd(zone);
    return result;
}


void _parse_error(ParserContext* context, SourceLocation loc, char* fmt, ...) {
    context->bad_location = loc;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(context->error_message, sizeof(context->error_message), fmt, ap);
    va_end(ap);

    longjmp(context->jump_state, 1);
}

static SourceLocation _srcloc(ParserContext* context) {
    // @TODO Handle out of bounds
    SourceLocation loc;
    loc.import_id = context->import->import_id;
    loc.position = context->head;
    return loc;
}

static bool _skip_space(ParserContext* context) {
    u32 head_start = context->head;
    while (context->head < context->import->text.len) {
        char rune = context->import->text.ptr[context->head];
        char rune2 = 0;
        if (context->head+1 < context->import->text.len) {
            rune2 = context->import->text.ptr[context->head+1];
        }
        if (rune == '/' && rune2 == '/') {
            context->head += 2;
            while(context->head < context->import->text.len) {
                char rune = context->import->text.ptr[context->head];
                context->head += 1;
                if(rune == '\n') {
                    break;
                }
            }
        } else if (rune == '/' && rune2 == '*') {
            context->head += 2;
            u32 depth = 1;
            while(context->head < context->import->text.len) {
                char rune = context->import->text.ptr[context->head];
                char rune2 = 0;
                if (context->head + 1 < context->import->text.len) {
                    rune2 = context->import->text.ptr[context->head+1];
                }
                context->head += 1;
                if(rune == '/' && rune2 == '*') {
                    context->head += 1;
                    depth++;
                    continue;
                }
                if(rune == '*' && rune2 == '/') {
                    context->head += 1;
                    depth--;
                    if(depth == 0)
                        break;
                }
            }
        } else if (rune == '\t' || rune == ' ' || rune == '\n' || rune == '\r' || rune == '\f') {
            context->head++;
        } else {
            break;
        }
    }

    return head_start != context->head;
}

static int _peek_rune(ParserContext* context, int n) {
    if (context->head + n >= context->import->text.len) {
        // _parse_error(context, (const TokenExt*)&context->stream->tokens[context->head], "Unexpected token, wanted '%s'", name_from_token(kind));
        return EOF_RUNE;
    }
    // @TODO Handle out of bounds
    char current_rune = context->import->text.ptr[context->head + n];
    return current_rune;
}

static u32 _headpos(ParserContext* context) {
    if (context->head >= context->import->text.len) {
        SourceLocation loc = srcloc();
        // @TODO Fix this. does it happen?
        _parse_error(context, loc, "End of file, internal failure.");
        return -1;
    }
    return context->head;
}

static bool _match_rune(ParserContext* context, int rune, bool peek_operation) {
    if (context->head >= context->import->text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected end of file, wanted '%c'", rune);
        }
        return false;
    }
    char current_rune = context->import->text.ptr[context->head];
    if (current_rune != rune) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted '%c'", rune);
        }
        return false;
    }
    context->head++;
    return true;
}

static bool _match_word(ParserContext* context, const char* word, bool peek_operation) {
    if (context->head >= context->import->text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected end of file, wanted '%s'", word);
        }
        return false;
    }

    u32 word_len = strlen(word);
    u32 index = 0;
    while (index < context->import->text.len) {
        char chr = context->import->text.ptr[context->head + index];
        if (chr != word[index]) {
            if (!peek_operation) {
                SourceLocation loc = srcloc();
                _parse_error(context, loc, "Unexpected token, wanted '%s'", word);
            }
            return false;
        }
        index++;
        if (index == word_len) {
            break;
        }
    }

    context->head += index;

    return true;
}

static bool _match_string(ParserContext* context, string* outstring, bool peek_operation) {
    if (context->head >= context->import->text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted literal string.");
        }
        return false;
    }

    char rune = context->import->text.ptr[context->head];
    if (rune != '"') {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted literal string.");
        }
        return false;
    }
    u32 head = context->head + 1;

    string text = context->import->text;
    
    u32 head_start = head;
    while(head < text.len) {
        // Handle UTF-8 encoding here? or maybe nothing special is needed?
        char chr = text.ptr[head];
        head++;
        if(chr == '"' && text.ptr[head-2] != '\\') {
            break;
        }
    }
    // @TODO Handle if we broke out of loop because of EOF without ending quote.
    u32 head_end = head - 1;

    u32 str_length = head_end - head_start;
    string outstr = string_create(str_length);

    char* ptr = outstr.ptr;
    u32 index = 0;
    int real_count = 0;
    while (index < str_length) {
        char chr = text.ptr[head_start + index];
        index++;
        if (chr == '\\') {
            if (index < str_length) {
                char chr2 = text.ptr[head_start + index];
                if (chr2 == 'n') {
                    index++;
                    ptr[real_count] = '\n';
                    real_count++;
                    continue;
                } else if (chr2 == 't') {
                    index++;
                    ptr[real_count] = '\t';
                    real_count++;
                    continue;
                } else if (chr2 == 'r') {
                    index++;
                    ptr[real_count] = '\r';
                    real_count++;
                    continue;
                } else if (chr2 == 'e') {
                    index++;
                    ptr[real_count] = '\033';
                    real_count++;
                    continue;
                }  else if (chr2 == '"') {
                    index++;
                    ptr[real_count] = '"';
                    real_count++;
                    continue;
                }  else if (chr2 == '\'') {
                    index++;
                    ptr[real_count] = '\'';
                    real_count++;
                    continue;
                } else if (chr2 == '0') {
                    index++;
                    ptr[real_count] = '\0';
                    real_count++;
                    continue;
                } else if (chr2 == '\\') {
                    index++;
                    ptr[real_count] = '\\';
                    real_count++;
                    continue;
                } else if (chr2 == 'x') {
                    index++;
                    char c0 = text.ptr[head_start + index];
                    char c1 = text.ptr[head_start + index+1];
                    index+=2;
                    if (!is_hex(c0) || !is_hex(c1)) {
                        char* bad_text     = text.ptr + head_start + index - 4;
                        int   bad_text_len = (index) - (index - 4);
                        // @TODO Add location of error.
                        fprintf(stderr, "ERROR: Invalid escape sequence '%.*s'. (\\xXX)\n", bad_text_len, bad_text);
                        ASSERT(false);
                    }
                    ptr[real_count] = (hex_to_byte(c0) << 4) | hex_to_byte(c1);
                    real_count++;
                    continue;
                } else {
                    u32 codepoint;
                    if (chr2 == 'u') {
                        index++;
                        char c0 = text.ptr[head_start + index];
                        char c1 = text.ptr[head_start + index+1];
                        char c2 = text.ptr[head_start + index+2];
                        char c3 = text.ptr[head_start + index+3];
                        index += 4;
                        if (!is_hex(c0) || !is_hex(c1)|| !is_hex(c2) || !is_hex(c3)) {
                            char* bad_text     = text.ptr + head_start + index - 6;
                            int   bad_text_len = (index) - (index - 6);
                            // @TODO Add location of error.
                            fprintf(stderr, "ERROR: Invalid escape sequence '%.*s'. (\\uXXXX)\n", bad_text_len, bad_text);
                            ASSERT(false);
                        }
                        // @TODO Error check, c0,c1,c2,c3 should be [0-9a-fA-F]
                        codepoint = (hex_to_byte(c0) << 12) | (hex_to_byte(c1) << 8) | (hex_to_byte(c2) << 4) | (hex_to_byte(c3));
                        goto emit_codepoint;
                    } else if (chr2 == 'U') {
                        index++;
                        char c0 = text.ptr[head_start + index];
                        char c1 = text.ptr[head_start + index+1];
                        char c2 = text.ptr[head_start + index+2];
                        char c3 = text.ptr[head_start + index+3];
                        char c4 = text.ptr[head_start + index+4];
                        char c5 = text.ptr[head_start + index+5];
                        char c6 = text.ptr[head_start + index+6];
                        char c7 = text.ptr[head_start + index+7];
                        index += 8;
                        if (!is_hex(c0) || !is_hex(c1)|| !is_hex(c2) || !is_hex(c3)||
                        !is_hex(c4) || !is_hex(c5)|| !is_hex(c6) || !is_hex(c7)) {
                            char* bad_text     = text.ptr + head_start + index - 10;
                            int   bad_text_len = (index) - (index - 10);
                            // @TODO Add location of error.
                            fprintf(stderr, "ERROR: Invalid escape sequence '%.*s'. (\\uXXXX)\n", bad_text_len, bad_text);
                            ASSERT(false);
                        }
                        goto emit_codepoint;
                    }
                    if (false) {
                    emit_codepoint:
                        if (codepoint <= 0x7F) {
                            ptr[real_count] = codepoint;
                            real_count+=1;
                        } else if (codepoint <= 0x7FF) {
                            ptr[real_count]   = 0b11000000 | (codepoint >> 6);
                            ptr[real_count+1] = 0b10000000 | (codepoint & 0b111111);
                            real_count+=2;
                        } else if (codepoint <= 0xFFFF) {
                            ptr[real_count]   = 0b11100000 | (codepoint >> 12);
                            ptr[real_count+1] = 0b10000000 | ((codepoint >> 6) & 0b111111);
                            ptr[real_count+2] = 0b10000000 | (codepoint & 0b111111);
                            real_count+=3;
                        } else if (codepoint <= 0x10FFFF) {
                            ptr[real_count]   = 0b11100000 | (codepoint >> 18);
                            ptr[real_count+1] = 0b10000000 | ((codepoint >> 12) & 0b111111);
                            ptr[real_count+2] = 0b10000000 | ((codepoint >> 6) & 0b111111);
                            ptr[real_count+3] = 0b10000000 | (codepoint & 0b111111);
                            real_count+=4;
                        }
                        continue;
                    }
                }
            } else {
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", head_start);
                ASSERT(false);
            }
        }
        ptr[real_count] = chr;
        real_count++;
    }
    // @TODO Detect missing ending quote
    outstr.len = real_count;
    outstr.ptr[outstr.len] = '\0';
    context->head = head;
    *outstring = outstr;
    return true;
}

static bool _match_int(ParserContext* context, i64* out_value, bool peek_operation) {
    if (context->head >= context->import->text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected end of file, wanted literal int");
        }
        return false;
    }

    // @TODO match keyword
    
    string text = context->import->text;
    i64 accumulator = 0;
    u32 head = context->head;

    char rune = text.ptr[head];
    char rune2 = 0;
    if (head + 1 < text.len) {
        rune2 = text.ptr[head+1];
    }
    if (rune == '0' && rune2 == 'x') {
        head += 2;
        while(head < text.len) {
            char chr = text.ptr[head];
            if((chr >= '0' && chr <= '9')) {
                // @TODO Error for overflow
                accumulator = accumulator * 16 + chr - '0';
            } else if ((chr|32) >= 'a' && (chr|32) <= 'f') {
                accumulator = accumulator * 16 + (chr|32) - 'a' + 10;
            } else if (chr == '_') {
                // skip, _ is used for nice formatting in big numbers
            } else {
                break;
            }
            head++;
        }
    } else if (rune == '0' && rune2 == 'o') {
        head += 2;
        while(head < text.len) {
            char chr = text.ptr[head];
            if(chr >= '0' && chr <= '7') {
                accumulator = accumulator * 8 + chr - '0';
            } else if (chr == '_') {
                // skip
            } else {
                break;
            }
            head++;
        }
    } else if (rune == '0' && rune2 == 'b') {
        head += 2;
        while(head < text.len) {
            char chr = text.ptr[head];
            if(chr >= '0' && chr <= '1') {
                accumulator = accumulator * 2 + chr - '0';
            } else if (chr == '_') {
                // skip
            } else {
                break;
            }
            head++;
        }
    } else if(rune >= '0' && rune <= '9') {
        while(head < text.len) {
            char chr = text.ptr[head];
            if(chr >= '0' && chr <= '9') {
                accumulator = accumulator * 10 + chr - '0';
            } else if (chr == '_') {
                // skip
            } else {
                break;
            }
            head++;
        }
    } else {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted literal integer.");
        }
        return false;
    }

    context->head = head;
    *out_value = accumulator;
    return true;
}


static bool _consume_float_digits(
    string text,
    u32* head,
    double* value,
    bool fractional,
    u32* out_count
) {
    u32 count = 0;
    double place = 0.1;

    while (*head < text.len) {
        char chr = text.ptr[*head];

        if (chr >= '0' && chr <= '9') {
            i32 digit = chr - '0';

            if (fractional) {
                *value += digit * place;
                place *= 0.1;
            } else {
                *value = *value * 10.0 + digit;
            }

            count++;
            (*head)++;
        } else if (chr == '_') {
            // Underscores must separate digits.
            if (count == 0 ||
                *head + 1 >= text.len ||
                text.ptr[*head + 1] < '0' ||
                text.ptr[*head + 1] > '9') {
                return false;
            }

            (*head)++;
        } else {
            break;
        }
    }

    *out_count = count;
    return count > 0;
}


static bool _match_float(
    ParserContext* context,
    double* out_value,
    bool peek_operation
) {
    string text = context->import->text;

    // @TODO There may be rounding issues if we want to follow IEEE-754.

    if (context->head >= text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected end of file, wanted literal float.");
        }
        return false;
    }

    u32 head = context->head;
    double value = 0.0;
    u32 count = 0;

    // Integer part.
    if (!_consume_float_digits(text, &head, &value, false, &count)) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted literal float.");
        }
        return false;
    }

    bool is_float = false;

    // Fractional part.
    if (head < text.len && text.ptr[head] == '.') {
        is_float = true;
        head++;

        if (!_consume_float_digits(text, &head, &value, true, &count)) {
            goto invalid_float;
        }
    }

    // Scientific notation.
    if (head < text.len &&
        (text.ptr[head] == 'e' || text.ptr[head] == 'E')) {

        is_float = true;
        head++;

        bool negative_exponent = false;

        if (head < text.len &&
            (text.ptr[head] == '+' || text.ptr[head] == '-')) {
            negative_exponent = text.ptr[head] == '-';
            head++;
        }

        if (head >= text.len ||
            text.ptr[head] < '0' ||
            text.ptr[head] > '9') {
            goto invalid_float;
        }

        i32 exponent = 0;

        while (head < text.len) {
            char chr = text.ptr[head];

            if (chr >= '0' && chr <= '9') {
                // Prevent integer overflow on enormous exponents.
                if (exponent < 100000) {
                    exponent = exponent * 10 + chr - '0';

                    if (exponent > 100000) {
                        exponent = 100000;
                    }
                }
                head++;
            } else if (chr == '_') {
                if (head + 1 >= text.len ||
                    text.ptr[head + 1] < '0' ||
                    text.ptr[head + 1] > '9') {
                    goto invalid_float;
                }
                head++;
            } else {
                break;
            }
        }

        // Apply exponent using exponentiation by squaring.
        double scale = 10.0;
        double factor = 1.0;

        while (exponent > 0) {
            if (exponent & 1) {
                factor *= scale;
            }

            scale *= scale;
            exponent >>= 1;
        }

        if (negative_exponent) {
            value /= factor;
        } else {
            value *= factor;
        }
    }

    // Plain integers are not float literals.
    if (!is_float) {
        goto invalid_float;
    }

    context->head = head;
    *out_value = value;
    return true;

invalid_float:
    if (!peek_operation) {
        SourceLocation loc = srcloc();
        _parse_error(context, loc, "Invalid literal float.");
    }

/*
Test data to try
123.456	123.456
0.5	0.5
12.244e5	1224400
12e5	1200000
12e-5	0.00012
1_000.25	1000.25
1.5E+2	150
123	Not a float
12.	Invalid
12.abc	Invalid
12e	Invalid
12e+	Invalid
1__2.3	Invalid
*/

    return false;
}

static bool _match_id(ParserContext* context, cstring* outstring, bool peek_operation) {
    if (context->head >= context->import->text.len) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "EOF, wanted identifier");
        }
        return false;
    }
  
    string text = context->import->text;
    u32 head_start = context->head;
    u32 head = context->head;

    char rune = text.ptr[head];
    if(! (((rune|32) >= 'a' && (rune|32) <= 'z') || (unsigned)rune > 128)) {
        if (!peek_operation) {
            SourceLocation loc = srcloc();
            _parse_error(context, loc, "Unexpected token, wanted identifier.");
        }
        return false;
    }

    cstring outstr = {};
    outstr.ptr = text.ptr + head_start;

    while(head < text.len) {
        char rune = text.ptr[head];
        if(((rune|32) >= 'a' && (rune|32) <= 'z') || rune == '_' || (rune >= '0' && rune <= '9') || (unsigned)rune > 128) {
            head++;
        } else {
            break;
        }
    }
    outstr.len = head - head_start;
    context->head = head;

    *outstring = outstr;
    return true;
}


static void _advance(ParserContext* context) {
    if (context->head >= context->import->text.len)
        return;
    // @TODO Advance more than one byte if unicode character.
    context->head += 1;
}





