/*
    Entry point and command line parsing for the compiler CLI executable
*/

#include "basin/lexer.h"
#include "platform/string.h"
#include "platform/file.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("basin <source_file>\n");
        return 1;
    }

    string source_file;
    source_file.ptr = argv[1];
    source_file.len = strlen(argv[1]);

    string text = read_file(source_file.ptr);
    if(!text.ptr) {
        printf("Cannot read '%s'\n", source_file.ptr);
        return 1;
    }

    TokenStream* stream;
    Result result = tokenize(0, text, &stream);

    printf("result %d\n", result.kind);

    print_token_stream(stream);

    return 0;
}