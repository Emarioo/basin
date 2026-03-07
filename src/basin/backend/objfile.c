#include "basin/backend/objfile.h"

#include "basin/basin.h"
#include "basin/backend/ir.h"
#include "basin/backend/coff.h"

#include "platform/platform.h"


typedef struct ObjectContext {
    Compilation* compilation;

    // These are also stored in 'compilation' but cached here for easy access
    MachineProgram* machine_program;
    IRProgram* ir_program;
} ObjectContext;

void generate_coff(ObjectContext* context);
void generate_elf(ObjectContext* context);


void generate_object_file(Compilation* compilation) {
    printf("Gen object file %s\n", compilation->options->output_file);

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
            generate_elf(&context);
        } break;
        default:
            ASSERT(false);
    }

}


void print_compilation(ObjectContext* context) {

    printf("Sections [%d]\n", atomic_array_size(&context->ir_program->sections));
    for (int i=0;i<atomic_array_size(&context->ir_program->sections);i++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, i);

        printf("  %s, %u bytes\n", ir_section->name.ptr, ir_section->data_len);
    }
    
    printf("Functions [%d]\n", atomic_array_size(&context->machine_program->functions));
    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, function->id);

        printf("  %s, %u bytes\n", ir_function->name.ptr, function->code_len);
    }

    printf("Data Objects [%d]\n", atomic_array_size(&context->ir_program->variables));
    for (int i=0;i<atomic_array_size(&context->ir_program->variables);i++) {
        IRDataObject* ir_variable = atomic_array_getptr(&context->ir_program->variables, i);

        printf("  %s, %u bytes\n", ir_variable->name.ptr, ir_variable->size);
    }
}

void generate_coff(ObjectContext* context) {
    
    print_compilation(context);

    int fileOffset = 0;

    COFF_File_Header header = {};

    BasinTargetArch target_arch = determine_arch(context->compilation->options);
    
    switch (target_arch) {
        case BASIN_TARGET_ARCH_x86_64: header.Machine = IMAGE_FILE_MACHINE_AMD64; break;
        case BASIN_TARGET_ARCH_x86_32: header.Machine = IMAGE_FILE_MACHINE_I386;  break; 
        case BASIN_TARGET_ARCH_arm_64: header.Machine = IMAGE_FILE_MACHINE_ARM64; break;
        case BASIN_TARGET_ARCH_arm_32: header.Machine = IMAGE_FILE_MACHINE_ARM;   break;
        default:
            // @TODO Error message
            ASSERT(false);
    }
    
    u64 timestamp = time__now();

    header.Characteristics = 0;
    header.TimeDateStamp = timestamp / NANOSECOND_PER_SECOND;
    header.SizeOfOptionalHeader = 0;

    Section_Header exec_section = {};
    exec_section.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    strcpy(exec_section.Name, ".text");

    // @TODO WAAHH a lot of this code is hardcoded and won't work with constants, global variables, external functions, relocations!

    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        
        exec_section.SizeOfRawData += function->code_len;
        exec_section.NumberOfRelocations += array_size(&function->relocations);
    }

    header.NumberOfSections = 1 + atomic_array_size(&context->ir_program->sections);

    fileOffset += COFF_File_Header_SIZE + Section_Header_SIZE * header.NumberOfSections;

    exec_section.PointerToRawData = fileOffset;
    fileOffset += exec_section.SizeOfRawData;
    
    exec_section.PointerToRelocations = fileOffset;
    fileOffset += exec_section.NumberOfRelocations * COFF_Relocation_SIZE;

    header.PointerToSymbolTable = fileOffset;
    header.NumberOfSymbols = atomic_array_size(&context->ir_program->sections) + atomic_array_size(&context->machine_program->functions) + atomic_array_size(&context->ir_program->variables);

    int symbol_index_of_text_section = 0;

    const char* obj_path = context->compilation->options->output_file;

    FSHandle handle = fs__open(obj_path, FS_WRITE);
    if (handle == FS_INVALID_HANDLE) {
        log__printf("Could not write %s\n", obj_path);
        return;
    }

    // +1 because text section is first and not included in IR sections
    #define IR_SECTION_TO_SYMBOL_INDEX(ID) (ID+1)

    // +1 because text section is not included in ir sections
    int symbol_index_start_of_functions = 1 + atomic_array_size(&context->ir_program->sections);

    int exec_offset = exec_section.PointerToRawData;
    int rel_offset = exec_section.PointerToRelocations;
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
                    relocation.SymbolTableIndex = symbol_index_of_text_section;
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
            fs__write(handle, exec_section.PointerToRelocations, &relocation, COFF_Relocation_SIZE);
        }
    }

    int PointerToStringTable = header.PointerToSymbolTable + header.NumberOfSymbols * Symbol_Record_SIZE;
    int next_string_offset = 4;

    int current_strings_offset = 0;

    

    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, function->id);

        Symbol_Record symbol = {};
        symbol.SectionNumber = 1;
        symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
        symbol.Type = IMAGE_SYM_DTYPE_FUNCTION;
        symbol.NumberOfAuxSymbols = 0;
        symbol.Value = current_strings_offset;
        current_strings_offset += function->code_len;

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
        
        fs__write(handle, header.PointerToSymbolTable + i * Symbol_Record_SIZE, &symbol, Symbol_Record_SIZE);
    }

    
    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, function->id);

        Symbol_Record symbol = {};
        symbol.SectionNumber = 1;
        symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
        symbol.Type = IMAGE_SYM_DTYPE_FUNCTION;
        symbol.NumberOfAuxSymbols = 0;
        symbol.Value = current_strings_offset;
        current_strings_offset += function->code_len;

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
        
        fs__write(handle, header.PointerToSymbolTable + i * Symbol_Record_SIZE, &symbol, Symbol_Record_SIZE);
    }

    ASSERT(sizeof(next_string_offset) == 4);
    fs__write(handle, PointerToStringTable, &next_string_offset, sizeof(int));

    fs__write(handle, COFF_File_Header_SIZE, &exec_section, Section_Header_SIZE);

    fs__write(handle, 0, &header, COFF_File_Header_SIZE);

    fs__close(handle);
}


void generate_elf(ObjectContext* context) {
    ASSERT(false);
}

