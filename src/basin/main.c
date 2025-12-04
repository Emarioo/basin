/*
    Entry point and command line parsing for the compiler CLI executable

    Implementation for Basin API can be found in src/basin/basin.c
*/

#include "basin/basin.h"

void print_help();

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("basin  \033[32m%s\033[0m\n", basin_version(NULL));
        printf("commit \033[33m%s\033[0m\n", basin_commit());
        printf("built  \033[34m%s\033[0m\n", basin_build_date());
        return 1;
    }

    BasinCompileOptions options;
    memset(&options, 0, sizeof(options));

    BasinResult result;
    result = basin_parse_argv(argc, (const char**)argv, &options);

    if (result.error_type != BASIN_SUCCESS) {
        fprintf(stderr, "%s", result.error_message);
        return 1;
    }
    if (options.print_help) {
        print_help();
        return 0;
    }

    // @TEMP
    if (!options.output_file)
        options.output_file = "bin/dev.txt";

    // TODO: Measure compile time.
    //   Maybe put in result?

    result = basin_compile_file(options.input_file, options.output_file, &options);
    if (result.error_type == BASIN_SUCCESS) {
        printf("Success");
    } else if(result.error_type == BASIN_FILE_NOT_FOUND) {
        fprintf(stderr, "Cannot read '%s'\n", options.input_file);
    } else {
        fprintf(stderr, "Errors:");
        for(int i=0;i<result.compile_errors_len;i++) {
            fprintf(stderr, "%s", result.compile_errors[i]);
        }
    }

    if(result.error_message)
        basin_allocate(0, result.error_message, &options);
    if(result.compile_errors)
        basin_allocate(0, result.compile_errors, &options);

    return 0;
}

void print_help() {
    printf("Usage: basin [OPTIONS...] <input_file>my_file.bsn -o app.exe\n"
        "Options:\n"
        "  -o <path>    Path to output file\n"
        "  -g           Debug info\n"
        "  -O<N>        Optimize level (WIP, has no affect)\n");
        printf("\n");
    printf( "Example usage: basin my_file.bsn -o app.exe\n");
    printf("\n");
    printf("Version %s\n", basin_version(NULL));
    printf("\n");
    printf( "See source code at https://github.com/Emarioo/basin\n");
}