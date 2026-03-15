/*
    Entry point and command line parsing for the compiler CLI executable

    Implementation for Basin API can be found in src/basin/basin.c
*/

#include "basin/basin.h"
#include "util/assert.h"
#include "platform/platform.h"

void print_help();

void print_version() {
    printf("basin  \033[32m%s\033[0m\n", basin_version(NULL));
    printf("commit \033[33m%s\033[0m\n", basin_commit());
    printf("built  \033[34m%s\033[0m\n", basin_build_date());
}

int main(int argc, char** argv) {
    if(argc < 2) {
        print_version();
        return 1;
    }

    for (int i=0;i<argc;i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-help")) {
            print_help();
            return 0;
        } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-version")) {
            print_version();
            return 0;
        }
    }


    #ifdef TRACY_ENABLE
        // Sleep a little or tracy won't show advanced CPU stats
        // when running program as sudo/administrator
        thread__sleep_ns(100000000);
    #endif

    BasinCompileOptions options = {};

    BasinResult result;
    result = basin_parse_argv(argc, (const char**)argv, &options);

    if (result.error_type != BASIN_SUCCESS) {
        fprintf(stderr, "%s", result.error_message);
        return 1;
    }

    // @TEMP
    if (!options.output_file)
        options.output_file = "dev.o";

    // TODO: Measure compile time.
    //   Maybe put in result?

    result = basin_compile(&options);
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

    basin_free_result(&result);

    #ifdef TRACY_ENABLE
        // sleep so tracy has time to communicate the profiler data,
        // won't happen if we exit too quickly
        thread__sleep_ns(300000000);
    #endif

    return 0;
}

void print_help() {
    printf("Usage: basin [OPTIONS...] <input_file>\n"
        "Options: (WIP, most are not implemented)\n"
        "  -o <path>    Path to output file\n"
        "  -debug       Debug info\n"
        "  -run         Run program\n"
        "  -O <N>       Optimize level\n"
        "  -silent      Silence success and compile time info\n"
        "  -type        File code type. object, static library, executable...\n"
        "  -target      Short-hand target\n"
        "  -mos         Target OS\n"
        "  -march       Target Architecture\n"
        "  -mabi        Target ABI\n"
        "  -mformat     Target File Format\n"
        "  -dformat     Debug format\n"
        "  -mfeature    CPU extension features\n"
        "  -dump-ast    Dump AST\n"
        "  -dump-ir     Dump Intermediate Representation code\n"
        "  -dump-driver Dump compiler task scheduling\n"
    );
    printf("\n");
    printf( "Example usage: basin my_file.bsn -o app.exe\n");
    printf("\n");
    printf("Version %s\n", basin_version(NULL));
    printf("\n");
    printf( "See source code at https://github.com/Emarioo/basin\n");
}