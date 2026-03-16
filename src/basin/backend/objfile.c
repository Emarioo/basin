#include "basin/backend/objfile.h"

#include "basin/basin.h"
#include "basin/backend/ir.h"
#include "basin/backend/coff.h"

#include "platform/platform.h"


#define PROFILE_START() TracyCZone(zone, 1)

#define PROFILE_END() TracyCZoneEnd(zone)

typedef struct ObjectContext {
    Compilation* compilation;

    // These are also stored in 'compilation' but cached here for easy access
    MachineProgram* machine_program;
    IRProgram* ir_program;
} ObjectContext;

void generate_coff(ObjectContext* context);
void generate_elf(ObjectContext* context);


void generate_object_file(Compilation* compilation) {
    PROFILE_START()

    debug("Gen object file %s\n", compilation->options->output_file);

    ObjectContext context = {};
    context.compilation = compilation;
    context.machine_program = compilation->machine_program;
    context.ir_program = compilation->program;

    BasinTargetFormat format = determine_format(compilation->options);

    switch (format) {
        case BASIN_TARGET_FORMAT_COFF: {
            generate_coff(&context);
        } break;
        case BASIN_TARGET_FORMAT_ELF: {
            // generate_coff(&context);
            generate_elf(&context);
        } break;
        default:
            ASSERT(false);
    }
 
    PROFILE_END()
}


void print_compilation(ObjectContext* context) {
    

    printf("Sections [%d]\n", atomic_array_size(&context->ir_program->sections));
    for (int i=0;i<atomic_array_size(&context->ir_program->sections);i++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, i);

        printf("  %s, %d bytes\n", ir_section->name.ptr, (int)ir_section->data_len);
    }
    
    printf("Functions [%d]\n", atomic_array_size(&context->machine_program->functions));
    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, function->function_id);

        printf("  %s, %d bytes\n", ir_function->name.ptr, (int)function->code_len);
    }

    printf("Data Objects [%d]\n", atomic_array_size(&context->ir_program->variables));
    for (int i=0;i<atomic_array_size(&context->ir_program->variables);i++) {
        IRDataObject* ir_variable = atomic_array_getptr(&context->ir_program->variables, i);

        printf("  %s, %d bytes\n", ir_variable->name.ptr, (int)ir_variable->size);
    }
}

int get_machine_id_from_ir_id(ObjectContext* context, int ir_function_id) {
    // @TODO Create a map somewhere

    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* func = atomic_array_getptr(&context->machine_program->functions, i);
        if (func->function_id == ir_function_id) {
            return i;
        }
    }
    return -1;
}

int get_ir_id_from_machine_id(ObjectContext* context, int machine_function_id) {
    // @TODO Create a map somewhere
    MachineFunction* func = atomic_array_getptr(&context->machine_program->functions, machine_function_id);
    return func->function_id;
}


void generate_coff(ObjectContext* context) {
    PROFILE_START()

    if (should_debug_print()) {
        print_compilation(context);
    }

    const char* obj_path = context->compilation->options->output_file;
    FSHandle handle = fs__open(obj_path, FS_WRITE);
    if (handle == FS_INVALID_HANDLE) {
        log__printf("Could not write %s\n", obj_path);
        return;
    }

    int fileOffset = 0;

    COFF_File_Header header = {};

    BasinTargetArch target_arch = determine_arch(context->compilation->options);
    
    switch (target_arch) {
        case BASIN_TARGET_ARCH_x86_64: header.Machine = IMAGE_FILE_MACHINE_AMD64; break;
        case BASIN_TARGET_ARCH_i386: header.Machine = IMAGE_FILE_MACHINE_I386;  break; 
        case BASIN_TARGET_ARCH_aarch64: header.Machine = IMAGE_FILE_MACHINE_ARM64; break;
        case BASIN_TARGET_ARCH_arm: header.Machine = IMAGE_FILE_MACHINE_ARM;   break;
        default:
            // @TODO Error message
            ASSERT(false);
    }
    
    u64 timestamp = time__now_utc();

    header.Characteristics = 0;
    header.TimeDateStamp = timestamp / NANOSECOND_PER_SECOND;
    header.SizeOfOptionalHeader = 0;

    Section_Header exec_section = {};
    exec_section.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    strcpy(exec_section.Name, ".text");

    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        
        exec_section.SizeOfRawData += function->code_len;
        exec_section.NumberOfRelocations += array_size(&function->relocations);
    }

    header.NumberOfSections = atomic_array_size(&context->ir_program->sections);

    fileOffset += COFF_File_Header_SIZE + Section_Header_SIZE * header.NumberOfSections;
    
    exec_section.PointerToRawData = fileOffset;
    fileOffset += exec_section.SizeOfRawData;
    
    exec_section.PointerToRelocations = fileOffset;
    fileOffset += exec_section.NumberOfRelocations * COFF_Relocation_SIZE;

    fs__write(handle, COFF_File_Header_SIZE, &exec_section, Section_Header_SIZE);

    // @NOTE We start from i=1 because first section is stack which isn't a real section
    for (int i=1;i<atomic_array_size(&context->ir_program->sections);i++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, i);
        Section_Header section = {};
        section.Characteristics = IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        section.SizeOfRawData = ir_section->data_len;
        section.PointerToRawData = fileOffset;
        fileOffset += ir_section->data_len;
        // @TODO Align data according to ALGIN characetistics

        ASSERT(ir_section->name.len <= 8);
        memcpy(section.Name, ir_section->name.ptr, ir_section->name.len);
        if (ir_section->name.len < 8)
            section.Name[ir_section->name.len] = '\0';
        // @TODO Handle large section names

        if (ir_section->data_len > 0) {
            fs__write(handle, section.PointerToRawData, ir_section->data, ir_section->data_len);
        }

        fs__write(handle, COFF_File_Header_SIZE + (i) * Section_Header_SIZE, &section, Section_Header_SIZE);
    }

    header.PointerToSymbolTable = fileOffset;
    // header.NumberOfSymbols = atomic_array_size(&context->ir_program->sections) + atomic_array_size(&context->machine_program->functions) + atomic_array_size(&context->ir_program->variables);
    header.NumberOfSymbols = atomic_array_size(&context->ir_program->sections) + atomic_array_size(&context->ir_program->functions) + atomic_array_size(&context->ir_program->variables);

    fileOffset += header.NumberOfSymbols * Symbol_Record_SIZE;
    

    int symbol_index_of_text_section = 0;


    // +1 because text section is not included in IR section and stack section is not really a section
    #define IR_SECTION_TO_SYMBOL_INDEX(ID) (ID)

    // +2 because above
    int symbol_index_start_of_functions = atomic_array_size(&context->ir_program->sections);

    int exec_offset = exec_section.PointerToRawData;
    // for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        int function_data_offset = exec_offset;
        fs__write(handle, function_data_offset, function->code, function->code_len);
        exec_offset += function->code_len;

        for (int ri=0;ri<array_size(&function->relocations);ri++) {
            MachineRelocation* rel = array_getptr(&function->relocations, ri);
            COFF_Relocation relocation = {};
            switch (rel->type) {
                case RELOCATION_TYPE_FUNCTION: {
                    relocation.Type = IMAGE_REL_AMD64_REL32;
                    relocation.SymbolTableIndex = symbol_index_start_of_functions + rel->function_id;
                    relocation.VirtualAddress = rel->code_offset;
                } break;
                case RELOCATION_TYPE_DATA_OBJECT: {
                    relocation.Type = IMAGE_REL_AMD64_REL32;
                    relocation.SymbolTableIndex = IR_SECTION_TO_SYMBOL_INDEX(rel->section_id);
                    relocation.VirtualAddress = rel->code_offset;
                    int value = rel->value_offset;
                    fs__write(handle, function_data_offset + rel->code_offset, &value, sizeof(int));
                } break;
            }
            fs__write(handle, exec_section.PointerToRelocations + ri * COFF_Relocation_SIZE, &relocation, COFF_Relocation_SIZE);
        }
    }

    int PointerToStringTable = header.PointerToSymbolTable + header.NumberOfSymbols * Symbol_Record_SIZE;
    int next_string_offset = 4;

    int next_symbol_index = 0;

    {
        Symbol_Record symbol = {};
        symbol.SectionNumber = 1;
        symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
        symbol.Type = IMAGE_SYM_DTYPE_NULL;
        symbol.NumberOfAuxSymbols = 0;
        symbol.Value = 0;
        strcpy(symbol.Name.ShortName, ".text");

        fs__write(handle, header.PointerToSymbolTable + next_symbol_index * Symbol_Record_SIZE, &symbol, Symbol_Record_SIZE);
        next_symbol_index++;
    }

    for (int i=1;i<atomic_array_size(&context->ir_program->sections);i++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, i);

        Symbol_Record symbol = {};
        symbol.SectionNumber = IR_SECTION_TO_SYMBOL_INDEX(i)+1; // +1 because SectionNumber starts counting from 1.
        symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
        symbol.Type = IMAGE_SYM_DTYPE_NULL;
        symbol.NumberOfAuxSymbols = 0;
        symbol.Value = 0;

        if (ir_section->name.len > 8) {
            symbol.Name.zero = 0;
            symbol.Name.offset = next_string_offset;

            fs__write(handle, PointerToStringTable + next_string_offset, ir_section->name.ptr, ir_section->name.len+1);
            next_string_offset += ir_section->name.len+1;
        } else {
            memcpy(symbol.Name.ShortName, ir_section->name.ptr, ir_section->name.len);
            if (ir_section->name.len != 8) {
                symbol.Name.ShortName[ir_section->name.len] = '\0';
            }
        }
        
        fs__write(handle, header.PointerToSymbolTable + next_symbol_index * Symbol_Record_SIZE, &symbol, Symbol_Record_SIZE);
        next_symbol_index++;
    }

    int current_function_text_offset = 0;
    for (int fi=0;fi<atomic_array_size(&context->ir_program->functions);fi++) {
        IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, fi);
        int mi = get_machine_id_from_ir_id(context, fi);
        MachineFunction* function = NULL;
        if (mi != -1) {
            function = atomic_array_getptr(&context->machine_program->functions, mi);
        }

        Symbol_Record symbol = {};
        symbol.Type = IMAGE_SYM_DTYPE_FUNCTION;
        symbol.NumberOfAuxSymbols = 0;
        if (function) {
            symbol.SectionNumber = 1;
            // symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
            symbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
            symbol.Value = current_function_text_offset;
            current_function_text_offset += function->code_len;
            printf("avail %s\n", ir_function->name.ptr);
        } else {
            printf("ext %s\n", ir_function->name.ptr);
            symbol.SectionNumber = 0;
            symbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
        }

        if (ir_function->name.len > 8) {
            symbol.Name.zero = 0;
            symbol.Name.offset = next_string_offset;

            fs__write(handle, PointerToStringTable + next_string_offset, ir_function->name.ptr, ir_function->name.len+1);
            next_string_offset += ir_function->name.len+1;
        } else {
            memcpy(symbol.Name.ShortName, ir_function->name.ptr, ir_function->name.len);
            if (ir_function->name.len != 8) {
                symbol.Name.ShortName[ir_function->name.len] = '\0';
            }
        }
        
        fs__write(handle, header.PointerToSymbolTable + next_symbol_index * Symbol_Record_SIZE, &symbol, Symbol_Record_SIZE);
        next_symbol_index++;
    }

    // @TODO Data objects symbols

    ASSERT(sizeof(next_string_offset) == 4);
    fs__write(handle, PointerToStringTable, &next_string_offset, sizeof(int));


    fs__write(handle, 0, &header, COFF_File_Header_SIZE);

    fs__close(handle);
    
    PROFILE_END()
}


void generate_elf(ObjectContext* context) {
    PROFILE_START()

    ASSERT(false);
    
    PROFILE_END()
}

