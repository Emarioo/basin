
#include "basin/core/lexer.h"

#include "platform/memory.h"
#include "platform/string.h"
#include "platform/file.h"
#include "platform/array.h"

DEF_ARRAY(int)

Result tokenize(const Import* import, const string text, TokenStream** out_stream) {
    ASSERT(out_stream);

    TokenStream* stream = (TokenStream*)HEAP_ALLOC_OBJECT(TokenStream);
    stream->import = import;
    // if (optional_path) {
    //     stream->stream_path = *optional_path;
    // } else {
    //     char buffer[50];
    //     snprintf(buffer, sizeof(buffer), "<import_id %u>", (unsigned)import_id);
    //     stream->stream_path = alloc_string(buffer);
    // }

    stream->tokens_len = 0;
    stream->tokens_max = text.len / 2 + 10;
    stream->tokens = heap_alloc(stream->tokens_max * sizeof(Token));
    
    stream->data_len = 0;
    stream->data_max = text.len / 2 + 100;
    stream->data = heap_alloc(stream->data_max);

    // TODO: Optimize by reusing the int array per thread
    Array_int curly_depth;
    array_init_int(&curly_depth, 10);
    Array_int paren_depth;
    array_init_int(&paren_depth, 10);

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
                if(chr < '0' || chr > '9') {
                    break;
                }
                head++;
            }
            int word_end = head;

            RESERVE_TOKEN_EXT()

            TokenExt* new_token = (TokenExt*)&stream->tokens[stream->tokens_len];
            stream->tokens_len += EXT_TOKEN_PER_TOKEN;

            new_token->import_id = import->import_id;
            new_token->kind = T_LITERAL_NUMBER;
            new_token->position = cur_head;

            int word_count = word_end - word_start;

            if(word_count > 256) {
                // TODO: Proper error handling
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", word_count);
                assert(false);
            }

            RESERVE_DATA(word_count + 1 + 1);
            new_token->ptr_data = stream->data + stream->data_len;
            stream->data_len += word_count + 1 + 1;
            
            *(u8*)new_token->ptr_data = word_count;
            memcpy(new_token->ptr_data + 1, text.ptr + word_start, word_count);
            new_token->ptr_data[1 + word_count] = '\0';
            continue;
        }

        if (((c|32) >= 'a' && (c|32) <= 'z') || c == '_') {
            // identifier or keyword

            int word_start = cur_head;
            while(head < text.len) {
                char chr = text.ptr[head];
                if(!(chr >= '0' && chr <= '9') && !((chr|32) >= 'a' && (chr|32) <= 'z') && chr != '_') {
                    break;
                }
                head++;
                continue;
            }
            int word_end = head;

            string word = {0};
            word.ptr = text.ptr + word_start;
            word.len = word_end - word_start;

            // TODO: Optimize keyword comparison
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
        
                new_token->import_id = import->import_id;
                new_token->kind = T_IDENTIFIER;
                new_token->position = cur_head;

                int word_count = word_end - word_start;

                if(word_count > 256) {
                    // TODO: Proper error handling
                    fprintf(stderr, "ERROR: Identifiers cannot be larger than 256 characters (token was %d chars.)\n", word_count);
                    assert(false);
                }

                RESERVE_DATA(word_count + 1 + 1);
                new_token->ptr_data = stream->data + stream->data_len;
                stream->data_len += word_count + 1 + 1;
                
                new_token->ptr_data[0] = word_count;
                memcpy(new_token->ptr_data + 1, text.ptr + word_start, word_count);
                new_token->ptr_data[1 + word_count] = '\0';
            } else {
                RESERVE_TOKEN();

                Token* new_token = &stream->tokens[stream->tokens_len];
                stream->tokens_len++;
        
                new_token->import_id = import->import_id;
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

            new_token->import_id = import->import_id;
            new_token->kind = T_LITERAL_STRING;
            new_token->position = cur_head;

            int word_count = word_end - word_start;

            if(word_count > 65536) {
                // TODO: Proper error handling
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", word_count);
                ASSERT(false);
            }

            // TODO: Handle escape character

            RESERVE_DATA(word_count + 2 + 1);
            new_token->ptr_data = stream->data + stream->data_len;
            stream->data_len += word_count + 2 + 1;
            
            *(u16*)new_token->ptr_data = word_count;
            memcpy(new_token->ptr_data + 2, text.ptr + word_start, word_count);
            new_token->ptr_data[2 + word_count] = '\0';
            continue;
        }
        
        if(c == '{' || c == '}' || c == '(' || c == ')') {
            RESERVE_TOKEN_EXT()
            TokenExt* new_token = (void*)&stream->tokens[stream->tokens_len];
            int token_index = stream->tokens_len;
            stream->tokens_len += EXT_TOKEN_PER_TOKEN;
            
            new_token->import_id = import->import_id;
            new_token->kind = c;
            new_token->position = cur_head;
            switch(c) {
                case '{': {
                    new_token->int_data = -1; // set later
                    array_push_int(&curly_depth,&token_index);
                } break;
                case '}': {
                    int index = array_last(&curly_depth);
                    array_pop_int(&curly_depth);

                    ASSERT_INDEX(index < stream->tokens_len);
                    TokenExt* prev = (void*)&stream->tokens[index];

                    new_token->int_data = index;
                    prev->int_data = token_index;
                } break;
                case '(': {
                    new_token->int_data = -1; // set later
                    array_push_int(&paren_depth,&token_index);
                } break;
                case ')': {
                    int index = array_last(&paren_depth);
                    array_pop_int(&paren_depth);

                    ASSERT_INDEX(index < stream->tokens_len);
                    TokenExt* prev = (void*)&stream->tokens[index];

                    new_token->int_data = index;
                    prev->int_data = token_index;
                } break;
                default: ASSERT(false);
            }
        } else {
            RESERVE_TOKEN();
            Token* new_token = &stream->tokens[stream->tokens_len];
            stream->tokens_len++;

            new_token->import_id = import->import_id;
            new_token->kind = c;
            new_token->position = cur_head;
        }
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
        } else if(IS_SPECIAL(tok->kind)) {
            if(tok->kind == '{' || tok->kind == '}' || tok->kind == '(' || tok->kind == ')') {
                head += 2;
            } else {
                head++;
            }
            printf(" special, '%c', import %d, pos %d\n", (char)tok->kind, tok->import_id, tok->position);
        } else if (tok->kind == T_IDENTIFIER) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = tok->ptr_data[0];
            printf(" identifier, data '%.*s', import %d, pos %d\n", len, tok->ptr_data + 1, tok->import_id, tok->position);
        } else if (tok->kind == T_LITERAL_NUMBER) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = tok->ptr_data[0];
            printf(" number, data %.*s, import %d, pos %d\n", len, tok->ptr_data + 1, tok->import_id, tok->position);
        } else if (tok->kind == T_LITERAL_STRING) {
            head += EXT_TOKEN_PER_TOKEN;
            int len = *(u16*)tok->ptr_data;
            printf(" string, data \"%.*s\", import %d, pos %d\n", len, tok->ptr_data + 2, tok->import_id, tok->position);
        } else if(tok->kind == T_END_OF_FILE) {
            head++;
            printf(" EOF, import %d, pos %d\n", tok->import_id, tok->position);
        } else {
            fprintf(stderr, "%s: unhandled kind %d\n", __func__, tok->kind);
            assert(false);
        }
    }
}

bool compute_source_info(TokenStream* stream, TokenExt token, int* out_line, int* out_column, string* out_code) {
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
    if(token.kind == T_END_OF_FILE) {
        return false;
    }
    
    string text = read_file(stream->import->path.ptr);

    if(!text.ptr) {
        return false;
    }

    int head_at_line_start = 0;
    int line = 1;
    int column = 1;
    int nr_tabs = 0;
    int head = 0;
    while(head < text.len) {
        char chr = text.ptr[head];
        head++;

        if(chr == '\n') {
            if(head > token.position) {
                head--;
                break;
            }
            head_at_line_start = head;
            line++;
            column=1;
            nr_tabs=0;
            continue;
        }
        if(head < token.position) {
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
        out_code->max = out_code->len + 1; // +1 for null character
        out_code->ptr = heap_alloc(out_code->max);
        memcpy(out_code->ptr, code, len);
        out_code->ptr[len] = '\n';
        out_code->ptr[out_code->len] = '\0';

        char* arrow_code = out_code->ptr + len + 1;

        int arrow_head = 0;
        while(arrow_head < nr_tabs) {
            arrow_code[arrow_head++] += '\t';
        }
        while(arrow_head < column-1) {
            arrow_code[arrow_head++] += ' ';
        }
        arrow_code[arrow_head] = '^';
        arrow_code[arrow_head+1] = '\n';
    }

    heap_free(text.ptr);
    return true;
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
    "fn",
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
    "as",
    "in",
    "defer"
};