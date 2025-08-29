#pragma once

#include "basin/types.h"

//#################################
//      ENUMS AND STRUCTS
//#################################

typedef enum {
    // prefixed with T to prevent collisions (CONST collides with windows headers)
    T_END_OF_FILE = 0,
    T_IDENTIFIER,
    T_LITERAL_NUMBER,
    T_LITERAL_STRING,

    T_STRUCT, KEYWORD_BEGIN = T_STRUCT,
    T_FN,
    T_ENUM,
    T_GLOBAL,
    T_IMPORT,
    T_CONST,
    T_VAR,
    T_FOR,
    T_WHILE,
    T_SWTICH,
    T_IF,
    T_ELSE,
    T_AS,
    T_IN,
    T_DEFER, KEYWORD_END = T_DEFER + 1,

    // We can handle keywords from 0 to 32.
    // Then we have hashtag, dollar, parenthessis, dot, plus which are their own tokens.
    // then we have numbers, letters which we can use for a keyword spot.
    
} _TokenKind;

typedef u8 TokenKind;

extern char* keyword_table[KEYWORD_END - KEYWORD_BEGIN];

typedef struct {
    TokenKind kind;
    u8        _reserved; // maybe we want flags?
    ImportID  import_id; // do we need import id since we don't have preprocessor?
    int       position;
} Token;

typedef struct {
    TokenKind kind;
    u8        _reserved; // maybe we want flags?
    ImportID  import_id;
    int       position;

    union {
        char* ptr_data;
        int  int_data;
    };
} TokenExt;

#define IS_EXT_TOKEN(K) ((K >= IDENTIFIER && K <= LITERAL_STRING) || K == '{' || K == '}' || K == '(' || K == ')' )

#define IS_KEYWORD(K) (K >= KEYWORD_BEGIN && K < KEYWORD_END)

#define IS_SPECIAL(K) (K >= KEYWORD_END)

#define EXT_TOKEN_PER_TOKEN ((sizeof(TokenExt)+sizeof(Token)-1)/sizeof(Token))

typedef struct {
    const Import* import;

    Token* tokens;
    int tokens_len, tokens_max;

    char* data;
    int data_len, data_max;
} TokenStream;

static const Token EOF_TOKEN = { T_END_OF_FILE, 0, -1, -1 };
static const TokenExt EOF_TOKEN_EXT = { T_END_OF_FILE, 0, -1, -1, NULL };

//###############################
//       PUBLIC FUNCTIONS
//###############################

Result tokenize(const Import* import, const string text, TokenStream** out_stream);

// This is an expensive operation, we calculate line numbers
void print_lines_from_token_stream(TokenStream* stream, Token token);

bool compute_source_info(TokenStream* stream, TokenExt token, int* line, int* column, string* code);

void print_token_stream(TokenStream* stream);

void token_stream_cleanup(TokenStream* stream);