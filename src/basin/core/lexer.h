#pragma once

#include "basin/types.h"

//#################################
//      ENUMS AND STRUCTS
//#################################

typedef enum {
    END_OF_FILE = 256,
    IDENTIFIER,
    LITERAL_NUMBER,
    LITERAL_STRING,

    STRUCT, KEYWORD_BEGIN = STRUCT,
    ROUTINE,
    ENUM,
    GLOBAL,
    IMPORT,
    CONST,
    VAR,
    FOR,
    WHILE,
    SWTICH,
    IF,
    ELSE, KEYWORD_END = ELSE + 1,
} _TokenKind;

typedef u16 TokenKind;

extern char* keyword_table[KEYWORD_END - KEYWORD_BEGIN];

typedef struct {
    TokenKind kind;
    ImportID import_id;
    int position;
} Token;

typedef struct {
    TokenKind kind;
    ImportID import_id;
    int position;

    char* data;
} TokenExt;

#define IS_EXT_TOKEN(K) (K >= IDENTIFIER && K <= LITERAL_STRING)

#define IS_KEYWORD(K) (K >= KEYWORD_BEGIN && K < KEYWORD_END)

#define EXT_TOKEN_PER_TOKEN ((sizeof(TokenExt)+sizeof(Token)-1)/sizeof(Token))

typedef struct {
    Token* tokens;
    int tokens_len, tokens_max;
    
    // TokenExt* ext_tokens;
    // int ext_tokens_len, ext_tokens_max;
    
    char* data;
    int data_len, data_max;
} TokenStream;

//###############################
//       PUBLIC FUNCTIONS
//###############################

Result tokenize(const ImportID import_id, const string text, TokenStream** out_stream);

void print_token_stream(TokenStream* stream);

void token_stream_cleanup(TokenStream* stream);