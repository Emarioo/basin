#include "basin/basin.h"

#include "basin/core/driver.h"
#include "basin/logger.h"
#include "basin/error.h"

#include "util/string.h"
#include "util/assert.h"
#include "util/file.h"
#include "platform/platform.h"

static const char BASIN_COMPILER_VERSION[] = "0.0.1-dev";

// These are defined in 'bin/int/commit_hash.c' which is auto-generated file by build.py
extern const char* BASIN_COMPILER_COMMIT;
extern const char* BASIN_COMPILER_BUILD_DATE;

const char* basin_version(int version[3]) {
    if(version) {
        const char *start;
        char* end;
        int part;

        start = BASIN_COMPILER_VERSION;
        part = strtol(start, &end, 10);
        version[0] = part;
        
        start = end + 1; // +1 to skip dot
        part = strtol(start, &end, 10);
        version[1] = part;

        start = end + 1;
        part = strtol(start, &end, 10);
        version[2] = part;
    }
    return BASIN_COMPILER_VERSION;
}

const char* basin_commit() {
    return BASIN_COMPILER_COMMIT;
}
const char* basin_build_date() {
    return BASIN_COMPILER_BUILD_DATE;
}


BasinResult basin_compile(const BasinCompileOptions* options) {
    TracyCZone(zone, 1);
    
    BasinResult result = {};

    Driver* driver = driver_create();

    Compilation* comp = driver_create_compilation(driver, options);


    cstring c_path;
    if (options->input_file)
        c_path = cstr_cptr(options->input_file);
    else
        c_path = cstr_cptr("<unknown>");

    Task task = {};
    task.compilation = comp;
    task.kind = TASK_LEX_AND_PARSE;
    task.lex_and_parse.import = driver_create_import_id(driver, c_path);

    if (options->input_text) {
        task.lex_and_parse.import->text = string_clone(options->input_text, options->input_text_len);
    }

    driver_add_task(driver, &task);

    // 0, 0 means: Use all CPU threads and process all tasks
    driver_run(driver, 0, 0);

    // Driver finished compiling code
    // user metaprograms finished executing
    // Driver finished generating x86 (if that was specified), bytecode is finished at least
    // Driver spat out executable, shared library, bytecode or whatever was specified.
    
    // write it to a file

    driver_cleanup(driver);
    
    TracyCZoneEnd(zone);

    return result;
}

void basin_free_result(BasinResult* result) {
    if (result->compile_errors)
        mem__free(result->compile_errors);
    if (result->error_message)
        mem__free(result->error_message);
}



//#########################################
//            EXTRA FUNCTIONS
//#########################################


typedef char* cptr;
DEF_ARRAY(cptr)

BasinResult basin_parse_arguments(const char* arguments, BasinCompileOptions* options) {
    BasinResult result = { 0 };
    result.error_type = BASIN_SUCCESS;
    
    Array_cptr ptr_list;
    
    array_init(&ptr_list, 50);

    int head = 0;
    int len = strlen(arguments);
    bool in_string = 0;
    bool in_word = 0;
    int start_arg = 0;
    while (head<len) {
        char chr = arguments[head];
        head++;

        if (chr == '"') {
            if (in_string) {
                if (!in_word) {
                    int length = head-1 - start_arg;
                    char* arg = (char*)mem__alloc(length + 1);
                    memcpy(arg, arguments + start_arg, length);
                    arg[length] = '\0';
                    array_push(&ptr_list, &arg);
                }
                in_string = false;
                continue;
            } else {
                if (!in_word) {
                    start_arg = head;
                }
                in_string = true;
                continue;
            }
        }

        if (in_string)
            continue;

        if (chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r') {
            if (in_word) {
                int length = head-1 - start_arg;
                char* arg = (char*)mem__alloc(length + 1);
                memcpy(arg, arguments + start_arg, length);
                arg[length] = '\0';
                array_push(&ptr_list, &arg);
                in_word = false;
                continue;
            }
            continue;
        }

        if (!in_word) {
            start_arg = head-1;
            in_word = true;
        }

        if (head == len) {
            int length = head - start_arg;
            char* arg = (char*)mem__alloc(length + 1);
            memcpy(arg, arguments + start_arg, length);
            arg[length] = '\0';
            array_push(&ptr_list, &arg);
            in_word = false;
            continue;
        }
    }
    // @TODO Handle unterminated string
    // @TODO Handle escape characters in string

    result = basin_parse_argv(ptr_list.len, (const char**)ptr_list.ptr, options);

    for (int i=0;i<ptr_list.len;i++) {
        mem__free(ptr_list.ptr[i]);
    }

    array_cleanup(&ptr_list);

    return result;
}

BasinResult basin_parse_argv(int argc, const char** argv, BasinCompileOptions* options) {
    BasinResult result = { 0 };
    result.error_type = BASIN_SUCCESS;

    memset(options, 0, sizeof(*options));

    if (argc == -1) {
        argc++;
        while (argv[argc++]) ;
    }
    
    // @TODO The paths and user args we insert into options, should they be
    //    allocated anew or simplify use those passed in argv?
    //    If we allocate then user must free at some point.

    int argi = 1;
    while (argi < argc) {
        const char* arg = argv[argi];
        const char* value;
        argi++;

        #define COMBINE2(x,y) x ## y
        #define COMBINE(x,y) COMBINE2(x,y)

        #define DEF_ARG_CHOICE(FLAG,MISSING_ERROR)                                  \
            } else if(!strcmp(arg, FLAG)) {                                                    \
                if (argi >= argc) {                                                            \
                    FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, MISSING_ERROR, arg);   \
                    return result;                                                             \
                }                                                                              \
                value = argv[argi];                                                            \
                argi++;                                                                        \
                goto COMBINE(LABEL_ARG_, __LINE__);                                            \
            } else if(!strncmp(arg, FLAG "=", strlen(FLAG "="))) {                             \
                value = arg + strlen(FLAG "=");                                                \
                COMBINE(LABEL_ARG_, __LINE__):

        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            // ignore
        } else if(!strcmp(arg, "--")) {
            options->run_output_argv = argv;
            options->run_output_argc = argc - argi;
            break;
        } else if(!strcmp(arg, "-o")) {
            if (argi >= argc) {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Missing output path after '%s'\n", arg);
                return result;
            }
            options->output_file = argv[argi];
            argi++;
        } else if(!strncmp(arg, "-O", 2)) {
            options->optimize_flags = BASIN_OPTIMIZE_FLAG_all;
        } else if(!strcmp(arg, "-I")) {
            if (argi >= argc) {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Missing import directory after '%s'\n", arg);
                return result;
            }
            log__printf("WARNING: Currently ignoring -I, to be implemented\n");
            argi++;
        } else if(!strcmp(arg, "-L")) {
            if (argi >= argc) {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Missing library directory '%s'\n", arg);
                return result;
            }
            log__printf("WARNING: Currently ignoring -L, to be implemented\n");
            argi++;
        } else if(!strcmp(arg, "-debug")) {
            options->debug_flags |= BASIN_DEBUG_FLAG_full;
        } else if(!strncmp(arg, "-debug=", strlen("-debug="))) {
            value = arg + strlen("-debug=");
            if (!strcmp(value, "full")) {
                options->debug_flags |= BASIN_DEBUG_FLAG_full;
            } else if (!strcmp(value, "none")) {
                options->debug_flags = BASIN_DEBUG_FLAG_none;
            } else {
                // @TODO Print possible values
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown debug option '%s'.\n", value);
                return result;
            }

        DEF_ARG_CHOICE("-dformat", "ERROR: Missing debug format after '%s'. Formats: dwarf,pdb (pdb is not supported yet)\n")
            
            if (!strcmp(value, "dwarf")) {
                options->debug_flags = BASIN_DEBUG_FORMAT_dwarf;
            } else if (!strcmp(value, "pdb")) {
                options->debug_flags = BASIN_DEBUG_FORMAT_pdb;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown debug format '%s'. Formats: dwarf,pdb (pdb is not supported yet)\n", value);
                return result;
            }

       DEF_ARG_CHOICE("-type", "ERROR: Missing binary type after '%s'. Types: obj,lib,dyn,exe\n")
            
            if (!strcmp(value, "none")) {
                options->binary_output_type = BASIN_BINARY_none;
            } else if (!strcmp(value, "obj")) {
                options->binary_output_type = BASIN_BINARY_object_file;
            } else if (!strcmp(value, "lib")) {
                options->binary_output_type = BASIN_BINARY_static_library;
            } else if (!strcmp(value, "dyn")) {
                options->binary_output_type = BASIN_BINARY_dynamic_library;
                } else if (!strcmp(value, "exe")) {
                options->binary_output_type = BASIN_BINARY_executable;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown binary type '%s'. Types: obj,lib,dyn,exe\n", value);
                return result;
            }
    
        DEF_ARG_CHOICE("-mformat", "ERROR: Missing file format after '%s'. Formats: elf,coff\n")
        
            if (!strcmp(value, "elf")) {
                options->target_format = BASIN_TARGET_FORMAT_ELF;
            } else if (!strcmp(value, "coff")) {
                options->target_format = BASIN_TARGET_FORMAT_COFF;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown file format '%s'. Formats: elf,coff\n", value);
                return result;
            }

        DEF_ARG_CHOICE("-mos", "ERROR: Missing OS after '%s'. OSes: windows,linux\n")
        
            if (!strcmp(value, "windows")) {
                options->target_os = BASIN_TARGET_OS_windows;
            } else if (!strcmp(value, "linux")) {
                options->target_os = BASIN_TARGET_OS_linux;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown OS '%s'. Formats: windows,linux\n", value);
                return result;
            }
  
        DEF_ARG_CHOICE("-mabi", "ERROR: Missing ABI after '%s'. ABIs: msx64,sysv,eabi\n")
        
            if (!strcmp(value, "msx64")) {
                options->target_os = BASIN_TARGET_ABI_msx64;
            } else if (!strcmp(value, "sysv")) {
                options->target_os = BASIN_TARGET_ABI_sysv;
            } else if (!strcmp(value, "eabi")) {
                options->target_os = BASIN_TARGET_ABI_eabi;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown OS '%s'. ABIs: msx64,sysv,eabi\n", value);
                return result;
            }

        DEF_ARG_CHOICE("-march", "ERROR: Missing archtecture after '%s'. Architectures: i386,x86_64,arm,aarch64\n")
        
            if (!strcmp(value, "i386")) {
                options->target_arch = BASIN_TARGET_ARCH_i386;
            } else if (!strcmp(value, "x86_64")) {
                options->target_arch = BASIN_TARGET_ARCH_x86_64;
            } else if (!strcmp(value, "arm")) {
                options->target_arch = BASIN_TARGET_ARCH_arm;
            } else if (!strcmp(value, "aarch64")) {
                options->target_arch = BASIN_TARGET_ARCH_aarch64;
            } else {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown architecture '%s'. Architectures: i386,x86_64,arm,aarch64\n", value);
                return result;
            }
        
        } else if(!strcmp(arg, "-silent")) {
            options->silent = true;
        } else if(!strcmp(arg, "-run")) {
            options->run_output = true;
        } else if(arg[0] == '-') {
            FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Unknown argument '%s'. See --help\n", arg);
            return result;
        } else {
            if (options->input_file) {
                FORMAT_ERROR(result, BASIN_INVALID_COMPILE_OPTIONS, "ERROR: Multiple input files are not allowed, '%s'\n", arg);
                return result;
            }
            options->input_file = arg;
        }
    }

    return result;
}


const char* basin_target_arch_string(BasinTargetArch arch) {
    static const char* names[BASIN_TARGET_ARCH_COUNT] = {
        "host",
        "i386",
        "x86_64",
        "arm",
        "aarch64",
    };
    if (arch < 0 || arch >= BASIN_TARGET_ARCH_COUNT)
        return NULL;
    return names[arch];
}

const char* basin_target_os_string(BasinTargetOS os) {
    static const char* names[BASIN_TARGET_OS_COUNT] = {
        "host",
        "windows",
        "linux",
    };
    if (os < 0 || os >= BASIN_TARGET_OS_COUNT)
        return NULL;
    return names[os];
}