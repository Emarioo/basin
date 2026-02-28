#pragma once

#include "util/string.h"
#include "util/array.h"
#include "util/bucket_array.h"

DEF_ARRAY(string)

typedef u16 ImportID;

#define INVALID_IMPORT_ID 0xFFFFu


typedef struct TokenStream TokenStream;
typedef struct AST AST;

typedef struct BasinCompileOptions BasinCompileOptions;
typedef struct IRProgram IRProgram;
typedef struct IRSection IRSection;
typedef u8 IRSectionID;

typedef struct Driver Driver;

typedef struct {
    ImportID import_id;
    string path; // sometimes we don't have path, for small code created through metaprogramming for example.
    string text; // text may be empty, in this case the driver needs to look at path and read the file
    TokenStream* stream;
    AST* ast;
} Import;


DEF_BUCKET_ARRAY(Import)

/*
    A compilation with build options and internal structures.
    A driver normally has one but user can have a build script
    that submits multiple compilations.

    This also scales well with the API.

    Each normal compilation produces an object file.
    A non-normal compilation can:
      - run code JIT-style
      - link object file into executable
*/
typedef struct Compilation {
    const BasinCompileOptions* options;
    // build options
    //    output paths
    //    import paths
    //    optimization level
    //    debug level
    // initital source file or text
    
    // internal structures, AST, IR, Machine programs
    // indexes and IDs are relative to this struct

    IRProgram* program;
    // Cache them here so we don't have to look for them
    IRSection* section_rodata;
    IRSection* section_data;
    IRSectionID sectionid_rodata;
    IRSectionID sectionid_data;

    Array_string import_dirs;
    Array_string library_dirs;

    // Keeping driver here lets us pass around Compilation to functions
    // without also specifying the driver.
    Driver* driver;
} Compilation;

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;

typedef struct {
    const char* path;
    int line;
} CLocation;

#define CLOCATION { __FILE__, __LINE__ }

#define ARRAY_LENGTH(A) (sizeof(A)/sizeof(*(A)))

#ifdef _WIN32
    #define FL "%ll"
#else
    #define FL "%l"
#endif
