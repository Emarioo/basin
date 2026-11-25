#pragma once

#include "basin/util/types.h"


extern bool dump_driver_tasking;
extern bool dump_internal_data;
extern bool dump_parse_stage;
extern bool dump_genir_stage;
extern bool dump_codegen_stage;
extern bool dump_objfile_stage;

void dump_hex(void* address, int size, int stride);

#define debug(...) fprintf(stderr, __VA_ARGS__)
// #define debug(...)

#define DUMP_DRIVER_TASKING() dump_driver_tasking
#define DUMP_INTERNAL_DATA() dump_internal_data
#define DUMP_PARSE_STAGE() dump_parse_stage
#define DUMP_GENIR_STAGE() dump_genir_stage
#define DUMP_CODEGEN_STAGE() dump_codegen_stage
#define DUMP_OBJFILE_STAGE() dump_objfile_stage
