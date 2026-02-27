#include "basin/extra/c_transpiler.h"

#include "platform/array.h"

// @TODO Remove C lib dependencies
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>


#define ALLOC(SIZE) malloc(SIZE)
#define FREE(PTR) free(PTR)

typedef struct IfBlock {
    bool skip; // whether to skip current text in the current if,elif,else selective block
    bool in_else_block; // whether we have seen else, used to print error if we see a second one
    bool has_enabled_block; // whether an if, elif block was enabled, if so the other elif sections should be skipped
} IfBlock;

DEF_ARRAY(IfBlock)

typedef struct Context {
    CAST* ast;
    
    const char* text;
    int text_len;
    int text_cap;

    Array_IfBlock if_blocks;
} Context;

bool cts__tokenize(Context* context);
bool cts__preprocess(Context* context);
bool cts__parse(Context* context);


static int parse_space(const char* text, int text_len, int* head);
static int parse_name(const char* text, int text_len, int* head, char* name);
static int parse_string(const char* text, int text_len, int* head, char* name);
static int parse_int(const char* text, int text_len, int* head, int* value);

static void parse_error(Context* context, const char* text, int head, ...);

#define PARSE_SPACE() parse_space(text, text_len, &head)
#define PARSE_NAME(NAME) parse_name(text, text_len, &head, NAME)

CAST* cts__compile(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Cannot read %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* text = ALLOC(size+1);
    text[size] = '\0';

    fread(text, 1, size, file);

    FREE(text);

    return cts__compile_text(text, size);
}

CAST* cts__compile_text(const char* text, int size) {
    Context context = {};
    bool res;
    
    context.text = text;
    context.text_len = size;
    array_init(&context.blocks, 10);

    context.ast = ALLOC(sizeof(CAST));
    memset(context.ast, 0, sizeof(*context.ast));

    //
    // PREPROCESS
    //
    res = cts__preprocess(&context);
    if (!res) {
        return NULL;
    }

    //
    // TOKENIZE
    //
    res = cts__tokenize(&context);
    if (!res) {
        return NULL;
    }

    // 
    // PARSE
    // 
    res = cts__parse(&context);
    if (!res) {
        return NULL;
    }

    return NULL;
}


bool cts__preprocess(Context* context) {

    const char* text = context->text;
    const char* text_len = context->text_len;

    char* output = ALLOC(text_len);
    int output_len = 0;
    int output_cap = text_len;

    
    int head = 0;
    while (head < text_len) {
        const char chr = text[head];
        head++;

        if(text[head] == '\r' || text[head] == '\t' || text[head] == ' ' || text[head] == '\n') {
            continue;
        }

        // 
        // Comments
        // 
        if (chr == '/' && head+1 < text_len && text[head+1] == '/') {
            // @TODO Submit comment
            head++;
            
            while (head < text_len && text[head] != '\n') {
                head++;
            }
            continue;
        }
        
        if (chr == '/' && head+1 < text_len && text[head+1] == '*') {
            // @TODO Submit comment
            head++;
            
            while (head < text_len && text[head] != '*' && (head+1 >= text_len || text[head+1] != '/')) {
                head++;
            }

            continue;
        }

        //
        // Strings
        // 
        if (chr == '"') {
            head++;
            
            while (head < text_len && text[head] != '"') {
                // @TODO Handle escape codes
                head++;
            }
            
            if (text[head] != '"') {
                fprintf(stderr, "Unterminated quote. @TODO provide where quote started\n");
                return false;
            }
            head++;

            continue;
        }
        if (chr == '\'') {
            head++;
            
            while (head < text_len && text[head] != '\'') {
                // @TODO Handle escape codes
                head++;
            }
            
            if (text[head] != '\'') {
                fprintf(stderr, "Unterminated quote. @TODO provide where quote started\n");
                return false;
            }
            head++;

            continue;
        }

        // Directives
        if (chr != '#') {
            // @TODO Evaluate macros
            output[output_len] = chr;
            output_len++;

            continue;
        }
        
        PARSE_SPACE();

        char directive_name[256]; // @TODO PREVENT BUFFER OVERFLOW
        int directive_name_len = PARSE_NAME(directive_name);

        if (directive_name_len == 0) {
            // No alphanumeric after #
            continue;
        }

        typedef enum IfKind {
            I_NONE,
            I_IFDEF,
            I_IF,
            I_IFNDEF,
            I_ELIF,
            I_ELIFDEF,
            I_ELIFNDEF,
        } IfKind;

        IfKind if_kind = I_NONE;
        if (!strcmp(directive_name, "ifdef")) {
            if_kind = I_IFDEF;
        } else if (!strcmp(directive_name, "if")) {
            if_kind = I_IF;
        }

        if (if_kind != I_NONE) {
            if (if_kind == I_IF) {
                IfBlock block = {};
                array_push(&context->if_blocks, &block);
            }

            IfBlock* last_block = array_lastptr(&context->if_blocks);
            
            if (if_kind != I_IF && last_block->in_else_block) {
                fprintf(stderr, "Syntax error, #elif not allowed after #else\n");
                return false;
            }

            if (last_block->has_enabled_block || (context->if_blocks.len >= 2 && context->if_blocks.ptr[context->if_blocks.len-2].skip)) {
                // if previous if, elif block was enabled then always skip remaining elifs
                // if parent if block was skipped then we should always skip too
                last_block->skip = true;
            } else if (if_kind == I_IF || if_kind == I_ELIF) {
                int expr_start = head;
                // @TODO EVALUATE EXPRESSION
            } else {
                char name[256];
                int name_len = PARSE_NAME(name);

                if (name_len == 0) {
                    fprintf(stderr, "Syntax error, #ifdef/#ifndef expects macro name.\n");
                    return false;
                }
            }
        }

        if (!strcmp(directive_name, "else")) {
            
        } else if (!strcmp(directive_name, "endif")) {
            
        }
        
        if (!strcmp(directive_name, "define")) {

        } else if (!strcmp(directive_name, "undef")) {
            
        } else if (!strcmp(directive_name, "include")) {
            
        } else if (!strcmp(directive_name, "pragma")) {
            
        } else if (!strcmp(directive_name, "error")) {
            
        } else if (!strcmp(directive_name, "warning")) {
            
        }

        fprintf(stderr, "Unknown directive.\n");
        return false;
    }
    
    context->text = output;
    context->text_len = output_len;
    context->text_cap = output_cap;

    return true;
}

bool cts__tokenize(Context* context) {

    

    return true;
}


bool cts__parse(Context* context) {

    

    return true;
}



static void parse_error(Context* context, const char* text, int head, const char* format, ...) {

    int line = 0;
    int column = 0;

    // printf();

    fprintf(stderr, "ERROR:");

    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
    
}



int parse_space(const char* text, int text_len, int* head) {
    int start = *head;
    while(*head < text_len) {
        char c = text[*head];
        if(c != ' ' && c != '\t') {
        // if(c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        *head += 1;
    }
    return *head - start;
}

int parse_name(const char* text, int text_len, int* head, char* name) {
    int start = *head;
    while(*head < text_len) {
        char c = text[*head];
        if (!( 
            (((c|32) >= 'a' && (c|32) <= 'z')) ||
            (c == '_') ||
            (start != *head && c >= '0' && c <= '9')
            )) {
            break;
        }
        *head += 1;
    }
    if(name) {
        int len = *head - start;
        memcpy(name, text + start, len);
        name[len] = '\0';
    }
    return *head - start;
}
int parse_string(const char* text, int text_len, int* head, char* name) {
    if(text[*head] != '"') {
        return 0;
    }
    *head += 1;

    int start = *head;
    while(*head < text_len) {
        char c = text[*head];
        if(c == '"') {
            *head += 1;
            break;
        }
        *head += 1;
    }

    int len = *head - start - 1;
    memcpy(name, text + start, len);
    name[len] = '\0';
    return *head - start;
}
// DOES NOT RETURN PARSED INTEGER, check 'value' instead
int parse_int(const char* text, int text_len, int* head, int* value) {
    int start = *head;
    // while(*head < text.len) {
    //     char c = text.ptr[*head];
    //     if (c >= '0' && c <= '9') {
    //         *head += 1;
    //         continue;
    //     }
    //     break;
    // }
    // // *value = atoi(text.ptr + start);
    char* end_ptr;
    *value = strtol(text + *head, &end_ptr, 0);
    *head = (u64)end_ptr - (u64)text;

    while(*head < text_len) {
        char c = text[*head];
        if((c|32) == 'l' || (c|32) == 'u') {
            *head += 1;
            continue;
        }
        break;
    }
    return *head - start;
}
