/*
    Entry point and command line parsing for the compiler CLI executable

    Implementation for Basin API can be found in src/basin/basin.c
*/

#include "basin/basin.h"

void print_help();

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("basin  \033[32m%s\033[0m\n", basin_version(NULL));
        printf("commit \033[33m%s\033[0m\n", basin_commit());
        printf("built  \033[34m%s\033[0m\n", basin_build_date());
        return 1;
    }


    BasinCompileOptions options;
    memset(&options, 0, sizeof(options));

    const char* source_file = argv[1];
    const char* output_file = "bin/dev.txt";

    int start_of_user_arguments = -1;

    int argi = 1;
    while(argi<argc) {
        char* arg = argv[argi];
        argi++;

        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_help();
            return 0;
        } else if(!strcmp(arg, "--")) {
            start_of_user_arguments = argi;
            break;
        } else if(!strcmp(arg, "-o")) {
            if (argi >= argc) {
                fprintf(stderr, "ERROR: Missing output path after '%s'\n", arg);
                return 1;
            }
            output_file = argv[argi];
            argi++;
        } else if(!strncmp(arg, "-O", 2)) {
            options.optimize_flags = BASIN_OPTIMIZE_FLAG_all;
        } else if(!strcmp(arg, "-g")) {
            options.disable_debug = false;
        // } else if(!strncmp(arg, "-I", 2)) {
        //     int len = strlen(arg);
        //     if (len == 2) {
        //         if (argi >= argc) {
        //             fprintf(stderr, "ERROR: Missing output path after '%s'\n", arg);
        //             return 1;
        //         }
        //     }
        //     output_file = argv[argi];
        //     argi++;
        } else if(arg[0] == '-') {
            fprintf(stderr, "ERROR: Unknown argument '%s'. See --help\n", arg);
            return 1;
        } else {
            if (source_file) {
                fprintf(stderr, "ERROR: Multiple source files are not allowed, '%s'\n", arg);
                return 1;
            }
            source_file = arg;
        }
    }


    // TODO: Measure compile time.
    //   Maybe put in result?

    BasinResult result = basin_compile_file(source_file, output_file, &options);
    if (result.error_type == BASIN_SUCCESS) {
        printf("Success");
    } else if(result.error_type == BASIN_FILE_NOT_FOUND) {
        printf("Cannot read '%s'\n", source_file);
    } else {
        printf("Errors:");
        for(int i=0;i<result.compile_errors_len;i++) {
            printf("%s", result.compile_errors[i]);
        }
    }

    if(result.error_message)
        basin_allocate(0, result.error_message, &options);
    if(result.compile_errors)
        basin_allocate(0, result.compile_errors, &options);

    return 0;
}

void print_help() {
    printf( "Example usage: basin my_file.bsn -o app.exe\n"
            "Options:\n"
            "  -o <path>    Path to output file\n"
            "  --optimize <path>    Path to output file\n");

    printf("Version %s", basin_version(NULL));

    printf( "See source code at https://github.com/Emarioo/basin\n");
}