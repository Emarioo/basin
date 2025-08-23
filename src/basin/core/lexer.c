
#include "basin/core/lexer.h"

#include "platform/memory.h"

Result tokenize(const ImportID import_id, const string text, TokenStream** out_stream) {
    ASSERT(out_stream);

    TokenStream* stream = (TokenStream*)HEAP_ALLOC_OBJECT(TokenStream);

    stream->tokens_max = text.len / 2 + 10;
    stream->tokens = heap_alloc(stream->tokens_max * sizeof(Token));
    
    // stream->ext_tokens_max = text.len / 4 + 10;
    // stream->ext_tokens = heap_alloc(stream->ext_tokens_max * sizeof(TokenExt));
    
    stream->data_max = text.len / 2 + 100;
    stream->data = heap_alloc(stream->data_max);

    #define RESERVE_TOKEN() if (stream->tokens_len + 1 > stream->tokens_max) { stream->tokens = heap_realloc(stream->tokens, (stream->tokens_max*2 + 20) * sizeof(Token)); }
    #define RESERVE_TOKEN_EXT() if (stream->tokens_len + EXT_TOKEN_PER_TOKEN > stream->tokens_max) { stream->tokens = heap_realloc(stream->tokens, (stream->tokens_max*2 + sizeof(TokenExt) + 20) * sizeof(Token)); }
    #define RESERVE_DATA(N) if (stream->data_len + (N) > stream->data_max) { stream->data = heap_realloc(stream->data, (stream->data_max*2 + (N) + 500)); }

    int head = 0;
    while(head < text.len) {
        int cur_head = head;
        char c = text.ptr[head];
        char c2 = head + 1 < text.len ? text.ptr[head + 1] : 0;
        char c3 = head + 2 < text.len ? text.ptr[head + 2] : 0;
        head++;

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f')
            continue;


        if (c=='/' && c2 == '/') {
            head++;
            while(head < text.len) {
                char chr = text.ptr[head];
                head++;
                if(chr == '\n') {
                    break;
                }
            }
            continue;
        }
        if (c=='/' && c2 == '*') {
            head++;
            while(head < text.len) {
                char chr = text.ptr[head];
                char chr2 = head+1 < text.len ? text.ptr[head+1] : 0;
                head++;
                if(chr == '*' && chr2 == '/') {
                    head++;
                    break;
                }
            }
            continue;
        }

        if(c >= '0' && c <= '9') {
            // number

            // TODO: Support hexidecimal
            // Support binary
            // Support octal
            // Support float
            // Support underscore
            int word_start = cur_head;
            while(head < text.len) {
                char chr = text.ptr[head];
                if(c < '0' || c > '9') {
                    break;
                }
                head++;
            }
            int word_end = head;

            RESERVE_TOKEN_EXT()

            TokenExt* new_token = (TokenExt*)&stream->tokens[stream->tokens_len];
            stream->tokens_len += EXT_TOKEN_PER_TOKEN;

            new_token->import_id = import_id;
            new_token->kind = LITERAL_NUMBER;
            new_token->position = cur_head;

            int word_count = word_end - word_start;

            if(word_count > 256) {
                // TODO: Proper error handling
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", word_count);
                assert(false);
            }

            RESERVE_DATA(word_count + 1 + 1);
            new_token->data = stream->data + stream->data_len;
            stream->data_len += word_count + 1 + 1;
            
            *(u8*)new_token->data = word_count;
            memcpy(new_token->data + 1, text.ptr + word_start, word_count);
            new_token->data[1 + word_count] = '\0';
            continue;
        }

        if (((c|32) >= 'a' && (c|32) <= 'z') || c == '_') {
            // identifier or keyword

            int word_start = cur_head;
            while(head < text.len) {
                char chr = text.ptr[head];
                if((chr >= '0' && chr <= '9') || ((chr|32) >= 'a' && (chr|32) <= 'z') || c == '_') {
                    head++;
                    continue;
                }
                break;
            }
            int word_end = head;

            string word = {0};
            word.ptr = text.ptr + word_start;
            word.len = word_end - word_start;

            TokenKind kind = KEYWORD_BEGIN;
            while (kind < KEYWORD_END) {
                if(string_equal_cstr(word, keyword_table[kind - KEYWORD_BEGIN])) {
                    break;
                }
                kind++;
            }

            if(kind == KEYWORD_END) {
                RESERVE_TOKEN_EXT();

                TokenExt* new_token = (TokenExt*)&stream->tokens[stream->tokens_len];
                stream->tokens_len += EXT_TOKEN_PER_TOKEN;
        
                new_token->import_id = import_id;
                new_token->kind = IDENTIFIER;
                new_token->position = cur_head;

                int word_count = word_end - word_start;

                if(word_count > 256) {
                    // TODO: Proper error handling
                    fprintf(stderr, "ERROR: Identifiers cannot be larger than 256 characters (token was %d chars.)\n", word_count);
                    assert(false);
                }

                RESERVE_DATA(word_count + 1 + 1);
                new_token->data = stream->data + stream->data_len;
                stream->data_len += word_count + 1 + 1;
                
                new_token->data[0] = word_count;
                memcpy(new_token->data + 1, text.ptr + word_start, word_count);
                new_token->data[1 + word_count] = '\0';
            } else {
                RESERVE_TOKEN();

                Token* new_token = &stream->tokens[stream->tokens_len];
                stream->tokens_len++;
        
                new_token->import_id = import_id;
                new_token->kind = kind;
                new_token->position = cur_head;
            }
            continue;
        }

        if(c == '"') {
            // string

            int word_start = cur_head + 1;
            while(head < text.len) {
                char chr = text.ptr[head];
                head++;
                if(chr == '"') {
                    break;
                }
            }
            int word_end = head - 1;

            RESERVE_TOKEN_EXT()

            TokenExt* new_token = (TokenExt*)&stream->tokens[stream->tokens_len];
            stream->tokens_len += EXT_TOKEN_PER_TOKEN;

            new_token->import_id = import_id;
            new_token->kind = LITERAL_STRING;
            new_token->position = cur_head;

            int word_count = word_end - word_start;

            if(word_count > 65536) {
                // TODO: Proper error handling
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", word_count);
                assert(false);
            }

            // TODO: Handle escape character

            RESERVE_DATA(word_count + 2 + 1);
            new_token->data = stream->data + stream->data_len;
            stream->data_len += word_count + 2 + 1;
            
            *(u16*)new_token->data = word_count;
            memcpy(new_token->data + 2, text.ptr + word_start, word_count);
            new_token->data[2 + word_count] = '\0';
            continue;
        }

        RESERVE_TOKEN();

        Token* new_token = &stream->tokens[stream->tokens_len];
        stream->tokens_len++;

        new_token->import_id = import_id;
        new_token->kind = c;
        new_token->position = cur_head;
    }

    *out_stream = stream;

    Result result;
    result.kind = SUCCESS;
    return result;
}

void print_token_stream(TokenStream* stream) {
    printf("TokenStream, %d tokens\n", stream->tokens_len);

    int head = 0;
    while(head < stream->tokens_len) {
        TokenExt* tok = (TokenExt*)&stream->tokens[head];
        if (IS_KEYWORD(tok->kind)) {
            head++;
            printf(" %s, import %d, pos %d\n", keyword_table[tok->kind - KEYWORD_BEGIN], tok->import_id, tok->position);
        } else if(tok->kind >= 0 && tok->kind < END_OF_FILE) {
            head++;
            printf(" special, '%c', import %d, pos %d\n", (char)tok->kind, tok->import_id, tok->position);
        } else if (tok->kind == IDENTIFIER) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = tok->data[0];
            printf(" identifier, data '%.*s', import %d, pos %d\n", len, tok->data + 1, tok->import_id, tok->position);
        } else if (tok->kind == LITERAL_NUMBER) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = tok->data[0];
            printf(" number, data %.*s, import %d, pos %d\n", len, tok->data + 1, tok->import_id, tok->position);
        } else if (tok->kind == LITERAL_STRING) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = *(u16*)tok->data;
            printf(" string, data \"%.*s\", import %d, pos %d\n", len, tok->data + 2, tok->import_id, tok->position);
        } else if(tok->kind == END_OF_FILE) {
            head++;
            printf(" EOF, import %d, pos %d\n", tok->import_id, tok->position);
        } else {
            fprintf(stderr, "%s: unhandled kind %d\n", __func__, tok->kind);
            assert(false);
        }
    }
}

void token_stream_cleanup(TokenStream* stream) {
    if(stream->tokens)
        heap_free(stream->tokens);
    if(stream->data)
        heap_free(stream->data);
    heap_free(stream);
}


char* keyword_table[KEYWORD_END - KEYWORD_BEGIN] = {
    "struct",
    "routine",
    "enum",
    "global",
    "import",
    "const",
    "var",
    "for",
    "while",
    "switch",
    "if",
    "else",
};