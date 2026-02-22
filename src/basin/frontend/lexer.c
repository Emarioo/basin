
#include "basin/frontend/lexer.h"

#include "platform/platform.h"

#include "util/string.h"
#include "util/array.h"

Result tokenize(const Import* import, TokenStream** out_stream) {
    TracyCZone(zone, 1);

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
    string text = {};
    text.ptr = import->text.ptr;
    text.len = import->text.len;

    stream->tokens_len = 0;
    stream->tokens_max = text.len / 2 + 10;
    stream->tokens = mem__alloc(stream->tokens_max * sizeof(Token));
    
    stream->data_len = 0;
    stream->data_max = text.len / 2 + 100;
    stream->data = mem__alloc(stream->data_max);

    array_init(&stream->line_positions, import->text.len / 40);

    // TODO: Optimize by reusing the int array per thread
    Array_int curly_depth;
    array_init(&curly_depth, 10);
    Array_int paren_depth;
    array_init(&paren_depth, 10);

    #define RESERVE_TOKEN() if (stream->tokens_len + 1 > stream->tokens_max) { stream->tokens = mem__realloc((stream->tokens_max*2 + 20) * sizeof(Token), stream->tokens); }
    #define RESERVE_TOKEN_EXT() if (stream->tokens_len + TOKEN_PER_EXT_TOKEN > stream->tokens_max) { stream->tokens = mem__realloc((stream->tokens_max*2 + sizeof(TokenExt) + 20) * sizeof(Token), stream->tokens); }
    #define RESERVE_DATA(N) if (stream->data_len + (N) > stream->data_max) { stream->data = mem__realloc((stream->data_max*2 + (N) + 500), stream->data); }


    #define ADD_TOKEN(KIND,POS) do {                                    \
            RESERVE_TOKEN();                                            \
            Token* new_token = &stream->tokens[stream->tokens_len];     \
            stream->tokens_len++;                                       \
            new_token->import_id = import->import_id;                   \
            new_token->flags = 0;                                       \
            new_token->kind = KIND;                                     \
            new_token->position = POS;                                  \
            if (had_newline) new_token->flags |= TF_PRE_NEWLINE;        \
            if (had_space)   new_token->flags |= TF_PRE_SPACE;          \
            had_newline = false; had_space = false;                     \
            added_normal_token = true;                                  \
        } while (0)
        
    #define ADD_EXT_TOKEN(KIND,POS) (TokenExt*)&stream->tokens[stream->tokens_len]; do { \
            RESERVE_TOKEN_EXT();                                                  \
            TokenExt* new_token = (TokenExt*)&stream->tokens[stream->tokens_len]; \
            stream->tokens_len += TOKEN_PER_EXT_TOKEN;                            \
            new_token->import_id = import->import_id;                             \
            new_token->flags = 0;                                                 \
            new_token->kind = KIND;                                               \
            new_token->position = POS;                                            \
            if (had_newline) new_token->flags |= TF_PRE_NEWLINE;                  \
            if (had_space)   new_token->flags |= TF_PRE_SPACE;                    \
            had_newline = false; had_space = false;                               \
            added_normal_token = false;                                           \
        } while (0)

    int fstring_level = 0;
    int brace_depth = 0;

    bool had_newline = false;
    bool had_space   = false;
    bool added_normal_token = false;
    int line = 1;
    int head = 0;

    array_push(&stream->line_positions, &head);

    #define UPDATE_POST_NEWLINE() (stream->tokens_len ? stream->tokens[stream->tokens_len-(added_normal_token ? 1 : TOKEN_PER_EXT_TOKEN)].flags |= TF_POST_NEWLINE : 0 )
    #define UPDATE_POST_SPACE() (stream->tokens_len ? stream->tokens[stream->tokens_len-(added_normal_token ? 1 : TOKEN_PER_EXT_TOKEN)].flags |= TF_POST_SPACE : 0 )

    while(head < import->text.len) {
        int cur_head = head;
        char c = text.ptr[head];
        char c2 = head + 1 < text.len ? text.ptr[head + 1] : 0;
        char c3 = head + 2 < text.len ? text.ptr[head + 2] : 0;
        head++;

        bool reached_end_brace = false;
        bool parse_fstring = false;
        if (fstring_level) {
            if (c == '{') {
                brace_depth++;
            }
            if (c == '}') {
                brace_depth--;
                if (brace_depth == 0) {
                    // parse normal characters again
                    parse_fstring = true;
                    reached_end_brace = true;
                }
            }
        }

        if (c == 'f' && c2 == '"') {
            // formatted string
            head++;
            fstring_level++;
            parse_fstring = true;
        }

        if (parse_fstring) {
            bool reached_brace = false;
            int word_start = head;
            while(head < text.len) {
                char chr = text.ptr[head];
                head++;

                if (chr == '{') {
                    brace_depth++;
                    reached_brace = true;
                    break;
                }
                if(chr == '"') {
                    fstring_level--;
                    break;
                }
                if(chr == '\n') {
                    array_push(&stream->line_positions, &head);
                    line++;
                }
            }
            int word_end = head - 1;
            int word_count = word_end - word_start;

            if (reached_end_brace && word_count > 0) {
                ADD_TOKEN(',', cur_head);
            }
            if ((!reached_end_brace && !reached_brace ) || word_count > 0) {
                TokenExt* new_token = ADD_EXT_TOKEN(T_LITERAL_STRING, cur_head);

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
            }

            if (reached_brace && (word_count > 0 || reached_end_brace)) {
                ADD_TOKEN(',', head-1);
            }
            continue;
        }

        if (c == '\n') {
            had_newline = true;
            UPDATE_POST_NEWLINE();
            array_push(&stream->line_positions, &head);
            line++;
            continue;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            had_space = true;
            UPDATE_POST_SPACE();
            continue;
        }

        if (c=='/' && c2 == '/') {
            had_space = true;
            head++;
            while(head < text.len) {
                char chr = text.ptr[head];
                head++;
                if(chr == '\n') {
                    had_newline = true;
                    UPDATE_POST_NEWLINE();
                    array_push(&stream->line_positions, &head);
                    line++;
                    break;
                }
            }
            continue;
        }
        if (c=='/' && c2 == '*') {
            had_space = true;
            UPDATE_POST_SPACE();
            head++;
            int depth = 1;
            while(head < text.len) {
                char chr = text.ptr[head];
                char chr2 = head+1 < text.len ? text.ptr[head+1] : 0;
                head++;
                if(chr == '\n') {
                    had_newline = true;
                    UPDATE_POST_NEWLINE();
                    array_push(&stream->line_positions, &head);
                    line++;
                }
                if(chr == '/' && chr2 == '*') {
                    head++;
                    depth++;
                    continue;
                }
                if(chr == '*' && chr2 == '/') {
                    head++;
                    depth--;
                    if(depth == 0)
                        break;
                }
            }
            continue;
        }

        if(c >= '0' && c <= '9') {
            // number

            int word_start = cur_head;
            if (c == '0' && c2 == 'x') {
                head++;
                while(head < text.len) {
                    char chr = text.ptr[head];
                    if((chr < '0' || chr > '9') && ((chr|32) < 'a' || (chr|32) > 'f')) {
                        break;
                    }
                    head++;
                }
            } else if (c == '0' && c2 == 'o') {
                head++;
                while(head < text.len) {
                    char chr = text.ptr[head];
                    if(chr < '0' || chr > '7') {
                        break;
                    }
                    head++;
                }
            } else if (c == '0' && c2 == 'b') {
                head++;
                while(head < text.len) {
                    char chr = text.ptr[head];
                    if(chr < '0' || chr > '1') {
                        break;
                    }
                    head++;
                }
            } else {
                while(head < text.len) {
                    char chr = text.ptr[head];
                    if(chr < '0' || chr > '9') {
                        break;
                    }
                    head++;
                }
            }
            int word_end = head;

            TokenExt* new_token = ADD_EXT_TOKEN(T_LITERAL_INTEGER, cur_head);

            int word_count = word_end - word_start;

            if(word_count > 256) {
                // TODO: Proper error handling
                fprintf(stderr, "ERROR: Strings cannot be larger than 65536 characters (token was %d chars.)\n", word_count);
                ASSERT(false);
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

            cstring word = {0};
            word.ptr = text.ptr + word_start;
            word.len = word_end - word_start;

            // TODO: Optimize keyword comparison
            TokenKind kind = KEYWORD_BEGIN;
            while (kind < KEYWORD_END) {
                if(string_equal_cstr(word, token_name_table[kind])) {
                    break;
                }
                kind++;
            }

            if(kind == KEYWORD_END) {
                TokenExt* new_token = ADD_EXT_TOKEN(T_IDENTIFIER, cur_head);

                int word_count = word_end - word_start;

                if(word_count > 256) {
                    // TODO: Proper error handling
                    fprintf(stderr, "ERROR: Identifiers cannot be larger than 256 characters (token was %d chars.)\n", word_count);
                    ASSERT(false);
                }

                RESERVE_DATA(word_count + 1 + 1);
                new_token->ptr_data = stream->data + stream->data_len;
                stream->data_len += word_count + 1 + 1;
                
                new_token->ptr_data[0] = word_count;
                memcpy(new_token->ptr_data + 1, text.ptr + word_start, word_count);
                new_token->ptr_data[1 + word_count] = '\0';
            } else {
                ADD_TOKEN(kind, cur_head);
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
                if (chr == '\n') {
                    array_push(&stream->line_positions, &head);
                    line++;
                }
            }
            int word_end = head - 1;

            TokenExt* new_token = ADD_EXT_TOKEN(T_LITERAL_STRING, cur_head);

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
        
        // if(c == '{' || c == '}' || c == '(' || c == ')') {
        //     int token_index = stream->tokens_len;
        //     TokenExt* new_token = ADD_EXT_TOKEN(c, cur_head);
        //     switch(c) {
        //         case '{': {
        //             new_token->int_data = -1; // set later
        //             array_push(&curly_depth, &token_index);
        //         } break;
        //         case '}': {
        //             int index = array_last(&curly_depth);
        //             array_pop(&curly_depth);

        //             ASSERT_INDEX(index < stream->tokens_len);
        //             TokenExt* prev = (void*)&stream->tokens[index];

        //             new_token->int_data = index;
        //             prev->int_data = token_index;
        //         } break;
        //         case '(': {
        //             new_token->int_data = -1; // set later
        //             array_push(&paren_depth,&token_index);
        //         } break;
        //         case ')': {
        //             if (paren_depth.len == 0) {
        //                 fprintf(stderr, "ERROR: Unterminated parenthesis.\n");
        //                 ASSERT(false);
        //             }
        //             int index = array_last(&paren_depth);
        //             array_pop(&paren_depth);

        //             ASSERT_INDEX(index < stream->tokens_len);
        //             TokenExt* prev = (void*)&stream->tokens[index];

        //             new_token->int_data = index;
        //             prev->int_data = token_index;
        //         } break;
        //         default: ASSERT(false);
        //     }
        // } else {
        // }
        ADD_TOKEN(c, cur_head);
    }

    *out_stream = stream;

    // print_token_stream(stream);

    Result result = {};
    result.kind = SUCCESS;

    TracyCZoneEnd(zone);
    return result;
}

void print_token_stream(TokenStream* stream) {
    printf("TokenStream, %d tokens\n", stream->tokens_len);

    bool less = true;

    int head = 0;
    while(head < stream->tokens_len) {
        TokenExt* tok = (TokenExt*)&stream->tokens[head];
        if (IS_KEYWORD(tok->kind)) {
            head++;
            if (less) {
                printf("%s", token_name_table[tok->kind]);
            } else {
                printf(" %s, import %d, pos %d\n", token_name_table[tok->kind], tok->import_id, tok->position);
            }
        } else if(IS_SPECIAL(tok->kind)) {
            if(tok->kind == '{' || tok->kind == '}' || tok->kind == '(' || tok->kind == ')') {
                head += 2;
            } else {
                head++;
            }
            if (less) {
                printf("%c", (char)tok->kind);
            } else {
                printf(" special, '%c', import %d, pos %d\n", (char)tok->kind, tok->import_id, tok->position);
            }
        } else if (tok->kind == T_IDENTIFIER) {
            head += TOKEN_PER_EXT_TOKEN;
            int len = tok->ptr_data[0];
            if (less) {
                printf("%.*s", len, tok->ptr_data + 1);
            } else {
                printf(" identifier, data '%.*s', import %d, pos %d\n", len, tok->ptr_data + 1, tok->import_id, tok->position);
            }
        } else if (tok->kind == T_LITERAL_INTEGER) {
            head += TOKEN_PER_EXT_TOKEN;
            int len = tok->ptr_data[0];
            
            if (less) {
                printf("%.*s", len, tok->ptr_data + 1);
            } else {
                printf(" number, data %.*s, import %d, pos %d\n", len, tok->ptr_data + 1, tok->import_id, tok->position);
            }
        } else if (tok->kind == T_LITERAL_STRING) {
            head += TOKEN_PER_EXT_TOKEN;
            int len = *(u16*)tok->ptr_data;
            if (less) {
                printf(" \"%.*s\"", len, tok->ptr_data + 2);
            } else {
                printf(" string, data \"%.*s\", import %d, pos %d\n", len, tok->ptr_data + 2, tok->import_id, tok->position);
            }
        } else if(tok->kind == T_END_OF_FILE) {
            head++;
             if (less) {
                printf(" EOF\n");
            } else {
                printf(" EOF, import %d, pos %d\n", tok->import_id, tok->position);
            }
        } else {
            fprintf(stderr, "%s: unhandled kind %d\n", __func__, tok->kind);
            ASSERT(false);
        }
    }
}

static int pos_to_line_index(TokenStream* stream, int position) {
    TracyCZone(zone, 1);
    int* data = stream->line_positions.ptr;
    int  len  = stream->line_positions.len;
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

bool compute_source_info(TokenStream* stream, SourceLocation location, int* out_line, int* out_column, string* out_code) {
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
    text.ptr = stream->import->text.ptr;
    text.len = stream->import->text.len;

    if(!text.ptr) {
        goto end;
    }

    int nr_tabs = 0;
    int line_index = pos_to_line_index(stream, location.position);
    int line = line_index + 1;
    int column = 1;
    int head_at_line_start = stream->line_positions.ptr[line_index];
    int head = head_at_line_start;

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

SourceLocation location_from_token(const TokenExt* tok) {
    SourceLocation loc = {};
    loc.position = tok->position;
    loc.import_id = tok->import_id;
    return loc;
}

void token_stream_cleanup(TokenStream* stream) {
    if(stream->tokens)
        mem__free(stream->tokens);
    if(stream->data)
        mem__free(stream->data);
    mem__free(stream);
}

const char* name_from_token(TokenKind kind) {
    static char temp[8];
    if (kind < 32) {
        return token_name_table[kind];
    }
    temp[0] = kind;
    temp[1] = '\0';
    return temp;
}

char* token_name_table[NORMAL_TOKEN_END] = {
    "eof",
    "identifier",
    "literal_integer",
    "literal_float",
    "literal_string",
    "struct",
    "fn",
    "enum",
    "global",
    "import",
    "library",
    "as",
    "from",
    "const",
    "var",
    "for",
    "while",
    "if",
    "else",
    "switch",
    "case",
    "default",
    "in",
    "return",
    "yield",
    "continue",
    "break",
    "defer",
};

