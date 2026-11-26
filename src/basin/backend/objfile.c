#include "basin/backend/objfile.h"

#include "basin/basin.h"
#include "basin/frontend/ir.h"
#include "basin/backend/coff.h"
#include "basin/backend/elf.h"

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


bool generate_object_file(Compilation* compilation) {
    PROFILE_START()

    if (DUMP_OBJFILE_STAGE()) {
        debug("Gen object file %s\n", compilation->options->output_file);
    }

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
 
    PROFILE_END()

    return true;
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

int get_machine_id_from_ir_id(ObjectContext* context, IRFunction_id ir_function_id) {
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

    if (DUMP_INTERNAL_DATA()) {
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

    // Sections include .stack which isn't something we represent in the ELF file.
    // We do +1 for text section and -1 for .stack.
    header.NumberOfSections = 1 - 1 + atomic_array_size(&context->ir_program->sections);

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
        int mi = get_machine_id_from_ir_id(context, (IRFunction_id)fi);
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
            if (DUMP_OBJFILE_STAGE()) {
                printf("avail %s\n", ir_function->name.ptr);
            }
        } else {
            if (DUMP_OBJFILE_STAGE()) {
                printf("ext %s\n", ir_function->name.ptr);
            }
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

    if (DUMP_INTERNAL_DATA()) {
        print_compilation(context);
    }

    const char* obj_path = context->compilation->options->output_file;
    FSHandle handle = fs__open(obj_path, FS_WRITE);
    if (handle == FS_INVALID_HANDLE) {
        log__printf("Could not write %s\n", obj_path);
        return;
    }

    size_t fileOffset = 0;
    int REGISTER_SIZE = 0;
    bool big_endian = false;

    BasinTargetArch target_arch = determine_arch(context->compilation->options);
    
    switch (target_arch) {
        case BASIN_TARGET_ARCH_x86_64: REGISTER_SIZE = 8; break;
        case BASIN_TARGET_ARCH_i386: REGISTER_SIZE = 4;  break; 
        case BASIN_TARGET_ARCH_aarch64: REGISTER_SIZE = 8; break;
        case BASIN_TARGET_ARCH_arm: REGISTER_SIZE = 4;   break;
        default:
            // @TODO Error message
            ASSERT(false);
    }
    

    int small_elf = REGISTER_SIZE != 8;

    #define ELF_SET(P,M,V) (small_elf ? ((P##32)->M=V) : ((P##64)->M=V))
    #define ELF_GET(P,M) (small_elf ? (P##32)->M : (P##64)->M)
    #define ELF_PTR(P) (small_elf ? (void*)(P##32) : (void*)(P##64))
    
    Elf64_Ehdr header64 = {};
    Elf32_Ehdr header32 = {};

    unsigned char* e_ident = small_elf ? header32.e_ident : header64.e_ident;

    int headerSize = small_elf ? sizeof(Elf32_Ehdr) : sizeof(Elf64_Ehdr);
    

    e_ident[EI_MAG0] = ELFMAG0;
    e_ident[EI_MAG1] = ELFMAG1;
    e_ident[EI_MAG2] = ELFMAG2;
    e_ident[EI_MAG3] = ELFMAG3;
    e_ident[EI_CLASS] = small_elf ? 1 : 2;
    e_ident[EI_DATA] = big_endian ? 2 : 1;
    e_ident[EI_VERSION] = EV_CURRENT;

    ELF_SET(&header, e_type, ET_REL);
    ELF_SET(&header, e_version, EV_CURRENT);
    
    switch (target_arch) {
        case BASIN_TARGET_ARCH_x86_64: {
            REGISTER_SIZE = 8;
            ELF_SET(&header, e_machine, EM_X86_64);
        } break;
        case BASIN_TARGET_ARCH_i386: {
            REGISTER_SIZE = 4;
            ELF_SET(&header, e_machine, EM_860);
        } break; 
        case BASIN_TARGET_ARCH_aarch64: {
            REGISTER_SIZE = 8;
            ELF_SET(&header, e_machine, EM_AARCH64);
        } break;
        case BASIN_TARGET_ARCH_arm: {
            REGISTER_SIZE = 4;
            ELF_SET(&header, e_flags, 0x5000000);
            ELF_SET(&header, e_machine, EM_ARM);
        } break;
        default:
            // @TODO Error message
            ASSERT(false);
    }
    int sectionSize = small_elf ? sizeof(Elf32_Shdr) : sizeof(Elf64_Shdr);
    
    ELF_SET(&header, e_ehsize, headerSize);
    ELF_SET(&header, e_shentsize, sectionSize);

    size_t fileOffset_sections = headerSize;

    ELF_SET(&header, e_shoff, fileOffset_sections);

    int num_sections = 0;
    num_sections += 1; // null section
    num_sections += 1; // text section
    num_sections += 1; // relocation text section
    num_sections += 1; // symbol table
    num_sections += 1; // string table for symbols
    num_sections += 1; // string table for sections
    num_sections += 2 * (atomic_array_size(&context->ir_program->sections) - 1); // 2* for relocation and the actual section, -1 because it includes stack section
    ELF_SET(&header, e_shnum, num_sections);

    Elf32_Shdr null_section32 = {};
    Elf64_Shdr null_section64 = {};

    Elf32_Shdr section_strtab_section32 = {};
    Elf64_Shdr section_strtab_section64 = {};

    Elf32_Shdr symbol_strtab_section32 = {};
    Elf64_Shdr symbol_strtab_section64 = {};

    Elf32_Shdr symtab_section32 = {};
    Elf64_Shdr symtab_section64 = {};
    

    int sectionIndex_secstrtab = 1;
    int sectionIndex_symstrtab = 2;
    int sectionIndex_symtab = 3;
    int sectionIndex_startOfOtherSections = 4;

    ELF_SET(&header, e_shstrndx, sectionIndex_secstrtab);
    
    
    {
        ELF_SET(&section_strtab_section, sh_type, SHT_STRTAB);
        ELF_SET(&section_strtab_section, sh_addralign, 1);
    }
    
    int symbolSize = small_elf ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym);

    {
        ELF_SET(&symtab_section, sh_type, SHT_SYMTAB);
        if(target_arch != BASIN_TARGET_ARCH_arm) {
            // Not sure why this isn't set for ARM BUT i had it for BTB and i think it was for a good reason.
            ELF_SET(&symtab_section,sh_flags, SHF_ALLOC);
        }
        ELF_SET(&symtab_section, sh_link, sectionIndex_symstrtab); // index to symtab section
        ELF_SET(&symtab_section, sh_addralign, REGISTER_SIZE);
        ELF_SET(&symtab_section, sh_entsize, symbolSize);
    }
    
    {
        ELF_SET(&symbol_strtab_section, sh_type, SHT_STRTAB);
        ELF_SET(&symbol_strtab_section, sh_addralign, 1);
    }
    
    
    typedef struct ElfSection {
        int sec_index;
        int rela_index;
        union {
            struct {
                Elf32_Shdr sec32;
                Elf32_Shdr rela32;
            };
            struct {
                Elf64_Shdr sec64;
                Elf64_Shdr rela64;
            };
        };
    } ElfSection;
    
    ElfSection elfSections[10];
    int elfSections_max = 10;
    int elfSections_len = 0;

    // ir sections include .stack which we don't want but it does not include text section
    // so it evens out making below comparison correct.
    ASSERT(atomic_array_size(&context->ir_program->sections) <= elfSections_max);

    ElfSection* text_section = &elfSections[elfSections_len];
    *text_section = (ElfSection){};
    text_section->sec_index = sectionIndex_startOfOtherSections;
    text_section->rela_index = sectionIndex_startOfOtherSections + 1;
    elfSections_len++;

    int relocationSize = small_elf ? sizeof(Elf32_Rela) : sizeof(Elf64_Rela);

    {
        ELF_SET(&text_section->sec,sh_type, SHT_PROGBITS);
        ELF_SET(&text_section->sec,sh_flags, SHF_ALLOC | SHF_EXECINSTR);
        
        switch (target_arch) {
            case BASIN_TARGET_ARCH_i386:
            case BASIN_TARGET_ARCH_x86_64: {
                ELF_SET(&text_section->sec,sh_addralign, 1);
            } break;
            case BASIN_TARGET_ARCH_arm:
            case BASIN_TARGET_ARCH_aarch64: {
                ELF_SET(&text_section->sec,sh_addralign, 4);
            } break;
            default: ASSERT(false);
        }

        ELF_SET(&text_section->rela, sh_type, SHT_RELA);
        ELF_SET(&text_section->rela, sh_flags, 0);
        ELF_SET(&text_section->rela, sh_link, sectionIndex_symtab); // index to symtab section
        ELF_SET(&text_section->rela, sh_info, text_section->sec_index); // index to section where relocations should be applied
        ELF_SET(&text_section->rela, sh_addralign, REGISTER_SIZE);
        ELF_SET(&text_section->rela, sh_entsize, relocationSize);
    }

    for (int si = 1; si < atomic_array_size(&context->ir_program->sections); si++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, si);
        ElfSection* section = &elfSections[elfSections_len];
        ASSERT(si == elfSections_len); // This is a requirement we assume everywhere in this code.
        elfSections_len++;
        *section = (ElfSection){};
        section->sec_index = sectionIndex_startOfOtherSections + 2 * si;
        section->rela_index = sectionIndex_startOfOtherSections + 2 * si + 1 ;

        ELF_SET(&section->sec,sh_type, SHT_PROGBITS);
        ELF_SET(&section->sec,sh_flags, SHF_ALLOC | SHF_EXECINSTR);
        ELF_SET(&section->sec,sh_addralign, 1);
        ELF_SET(&section->sec,sh_size, ir_section->data_len);
        
        int relocationCount = 0;

        ELF_SET(&section->rela, sh_type, SHT_RELA);
        ELF_SET(&section->rela, sh_flags, 0);
        ELF_SET(&section->rela, sh_link, sectionIndex_symtab); // index to symtab section
        ELF_SET(&section->rela, sh_info, section->sec_index); // index to section where relocations should be applied
        ELF_SET(&section->rela, sh_addralign, REGISTER_SIZE);
        ELF_SET(&section->rela, sh_entsize, relocationSize);
        ELF_SET(&section->rela, sh_size, relocationSize * relocationCount);
    }

    fileOffset = headerSize + sectionSize * num_sections;
    {
        ELF_SET(&section_strtab_section, sh_offset, fileOffset);
        size_t strtab_base = fileOffset;
        
        char* name = "";
        fs__write(handle, fileOffset, name, 1);
        fileOffset += 1;

        name = ".shstrtab";
        fs__write(handle, fileOffset, name, strlen(name) + 1);
        ELF_SET(&section_strtab_section, sh_name, fileOffset - strtab_base);
        fileOffset += strlen(name) + 1;
        
        name = ".strtab";
        fs__write(handle, fileOffset, name, strlen(name) + 1);
        ELF_SET(&symbol_strtab_section, sh_name, fileOffset - strtab_base);
        fileOffset += strlen(name) + 1;
        
        name = ".symtab";
        fs__write(handle, fileOffset, name, strlen(name) + 1);
        ELF_SET(&symtab_section, sh_name, fileOffset - strtab_base);
        fileOffset += strlen(name) + 1;
        
        name = ".text";
        fs__write(handle, fileOffset, name, strlen(name) + 1);
        ELF_SET(&text_section->sec, sh_name, fileOffset - strtab_base);
        fileOffset += strlen(name) + 1;
        
        name = ".rela.text";
        fs__write(handle, fileOffset, name, strlen(name) + 1);
        ELF_SET(&text_section->rela, sh_name, fileOffset - strtab_base);
        fileOffset += strlen(name) + 1;

        // start at 1 because first section is .stack
        for (int si=1;si<atomic_array_size(&context->ir_program->sections);si++) {
            IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, si);
            ElfSection* section = &elfSections[si];

            fs__write(handle, fileOffset, ir_section->name.ptr, strlen(ir_section->name.ptr) + 1);
            ELF_SET(&section->sec, sh_name, fileOffset - strtab_base);
            fileOffset += strlen(name) + 1;
            
            char buffer[128];
            size_t len = snprintf(buffer, sizeof(buffer), ".rela%s", ir_section->name.ptr);
            ASSERT(len < sizeof(buffer) - 1);
            fs__write(handle, fileOffset, buffer, len + 1);
            ELF_SET(&section->rela, sh_name, fileOffset - strtab_base);
            fileOffset += len + 1;
        }

        ELF_SET(&section_strtab_section, sh_size, fileOffset - strtab_base);
    }
    
    fileOffset = (fileOffset + 1) & ~(size_t)7; // 8-byte alignment

    {
        size_t fileOffset_symbols = fileOffset;
        ELF_SET(&symtab_section, sh_offset, fileOffset_symbols);
        ELF_SET(&symtab_section, sh_info, 1); // the first null symbol is local, rest is global

        int bind = STB_LOCAL;
            
        int numberOfSymbols = 1; // null symbol
        numberOfSymbols += atomic_array_size(&context->ir_program->sections);
        numberOfSymbols += atomic_array_size(&context->ir_program->functions);
        // numberOfSymbols += atomic_array_size(&context->ir_program->variables);

        ELF_SET(&symtab_section, sh_size, numberOfSymbols * symbolSize);
        
        fileOffset += numberOfSymbols * symbolSize;
        size_t fileOffset_strtab = fileOffset;

        ELF_SET(&symtab_section, sh_offset, fileOffset_strtab);

        
        int next_symbol_index = 0;
        #define ELFX_ST_INFO(A,B) (small_elf ? ELF32_ST_INFO(A,B) : ELF64_ST_INFO(A,B))

        {
            Elf32_Sym symbol32 = {};
            Elf32_Sym symbol64 = {};

            fs__write(handle, fileOffset_symbols + next_symbol_index * symbolSize, ELF_PTR(&symbol), symbolSize);
            next_symbol_index++;
        }

        {
            Elf32_Sym symbol32 = {};
            Elf32_Sym symbol64 = {};

            const char* name = ".text";
            fs__write(handle, fileOffset, name, strlen(name) + 1);
            size_t name_offset = fileOffset - fileOffset_strtab;
            fileOffset += strlen(name) + 1;

            ELF_SET(&symbol, st_name, name_offset);
            ELF_SET(&symbol, st_info, ELFX_ST_INFO(bind, STT_SECTION));
            ELF_SET(&symbol, st_shndx, text_section->sec_index);
            ELF_SET(&symbol, st_value, 0);
            
            if(bind == STB_LOCAL) {
                ELF_SET(&text_section->sec, sh_info, next_symbol_index);
            }

            fs__write(handle, fileOffset_symbols + next_symbol_index * symbolSize, ELF_PTR(&symbol), symbolSize);
            next_symbol_index++;
        }

        for (int si = 1; si < atomic_array_size(&context->ir_program->sections); si++) {
            IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, si);
            
            ElfSection* section = &elfSections[si];

            Elf32_Sym symbol32 = {};
            Elf32_Sym symbol64 = {};

            fs__write(handle, fileOffset, ir_section->name.ptr, ir_section->name.len + 1);
            size_t name_offset = fileOffset - fileOffset_strtab;
            fileOffset += ir_section->name.len + 1;

            ELF_SET(&symbol, st_name, name_offset);
            ELF_SET(&symbol, st_info, ELFX_ST_INFO(bind, STT_SECTION));
            ELF_SET(&symbol, st_shndx, section->sec_index);
            ELF_SET(&symbol, st_value, 0);
            
            if(bind == STB_LOCAL) {
                ELF_SET(&text_section->sec, sh_info, next_symbol_index);
            }

            fs__write(handle, fileOffset_symbols + next_symbol_index * symbolSize, ELF_PTR(&symbol), symbolSize);
            next_symbol_index++;
        }
        
        bind = STB_GLOBAL;

        // @TODO All functions should NOT be global.

        int current_function_text_offset = 0;
        for (int fi=0;fi<atomic_array_size(&context->ir_program->functions);fi++) {
            IRFunction* ir_function = atomic_array_getptr(&context->ir_program->functions, fi);
            int mi = get_machine_id_from_ir_id(context, (IRFunction_id)fi);
            MachineFunction* function = NULL;
            if (mi != -1) {
                function = atomic_array_getptr(&context->machine_program->functions, mi);
            }

            Elf32_Sym symbol32 = {};
            Elf32_Sym symbol64 = {};

            fs__write(handle, fileOffset, ir_function->name.ptr, ir_function->name.len + 1);
            size_t name_offset = fileOffset - fileOffset_strtab;
            fileOffset += ir_function->name.len + 1;

            ELF_SET(&symbol, st_name, name_offset);
            
            if (function) {
                current_function_text_offset += function->code_len;
                ELF_SET(&symbol, st_info, ELFX_ST_INFO(bind, STT_FUNC));
                ELF_SET(&symbol, st_shndx, text_section->sec_index);
                ELF_SET(&symbol, st_value, current_function_text_offset);
                ELF_SET(&symbol, st_size, function->code_len);
            } else {
                ELF_SET(&symbol, st_info, ELFX_ST_INFO(bind, STT_NOTYPE));
            }

            fs__write(handle, fileOffset_symbols + next_symbol_index * symbolSize, ELF_PTR(&symbol), symbolSize);
            next_symbol_index++;
        }

        // @TODO Data object symbols

        ASSERT(next_symbol_index == numberOfSymbols);

        
        ELF_SET(&symbol_strtab_section, sh_size, fileOffset - fileOffset_strtab);
    }


    // +1 because text section is not included in IR section and stack section is not really a section
    #undef IR_SECTION_TO_SYMBOL_INDEX
    #define IR_SECTION_TO_SYMBOL_INDEX(ID) (sectionIndex_startOfOtherSections + ID)

    // +2 because above
    int symbol_index_start_of_functions = 1 + atomic_array_size(&context->ir_program->sections);

    
    switch (target_arch) {
        case BASIN_TARGET_ARCH_i386:
        case BASIN_TARGET_ARCH_x86_64: {
            fileOffset = (fileOffset + 1) & ~(size_t)15; // 16-byte alignment
        } break;
        case BASIN_TARGET_ARCH_arm:
        case BASIN_TARGET_ARCH_aarch64: {
            fileOffset = (fileOffset + 1) & ~(size_t)15; // 16-byte alignment
        } break;
        default: ASSERT(false);
    }

    ELF_SET(&text_section->sec, sh_offset, fileOffset);

    size_t relocationCount = 0;
    size_t codeSize = 0;
    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);
        int function_data_offset = fileOffset;
        fs__write(handle, function_data_offset, function->code, function->code_len);
        fileOffset += function->code_len;

        codeSize += function->code_len;
        relocationCount += array_size(&function->relocations);
    }
    
    ELF_SET(&text_section->sec, sh_size, codeSize);
    ELF_SET(&text_section->rela, sh_size, relocationSize * relocationCount);
    
    fileOffset = (fileOffset + 1) & ~(size_t)15; // 16-byte alignment

    ELF_SET(&text_section->rela, sh_offset, fileOffset);

    for (int i=0;i<atomic_array_size(&context->machine_program->functions);i++) {
        MachineFunction* function = atomic_array_getptr(&context->machine_program->functions, i);

        for (int ri=0;ri<array_size(&function->relocations);ri++) {
            MachineRelocation* rel = array_getptr(&function->relocations, ri);
            Elf32_Rela rela32 = {};
            Elf64_Rela rela64 = {};
           
            ELF_SET(&rela, r_offset, rel->code_offset);
            int rel_type = 0;
            int symbolIndex = 0;
            // TODO: 64-bit ARM
            ASSERT(target_arch == BASIN_TARGET_ARCH_x86_64);
            switch (rel->type) {
                case RELOCATION_TYPE_FUNCTION: {
                    if(target_arch == BASIN_TARGET_ARCH_arm) {
                        rel_type = R_ARM_ABS32;
                    } else {
                        rel_type = R_X86_64_PC32;
                    }
                    symbolIndex = symbol_index_start_of_functions + rel->function_id;
                    ELF_SET(&rela, r_addend, -4);
                } break;
                case RELOCATION_TYPE_DATA_OBJECT: {
                    if(target_arch == BASIN_TARGET_ARCH_arm) {
                        rel_type = R_ARM_ABS32;
                    } else {
                        rel_type = R_X86_64_PC32;
                    }
                    symbolIndex = IR_SECTION_TO_SYMBOL_INDEX(rel->section_id);
                    size_t offsetIntoSection = rel->value_offset;
                    ELF_SET(&rela, r_addend, offsetIntoSection - 4);
                } break;
            }
            if(small_elf) {
                ELF_SET(&rela, r_info, ELF32_R_INFO(symbolIndex, rel_type));
            } else {
                ELF_SET(&rela, r_info, ELF64_R_INFO(symbolIndex, rel_type));
            }

            fs__write(handle, fileOffset, ELF_PTR(&rela), relocationSize);
            fileOffset += relocationSize;
        }
    }
    
    fileOffset = (fileOffset + 1) & ~(size_t)15; // 16-byte alignment

    // @NOTE We start from i=1 because first section is stack which isn't a real section
    for (int si = 1; si < atomic_array_size(&context->ir_program->sections); si++) {
        IRSection* ir_section = atomic_array_getptr(&context->ir_program->sections, si);
        ElfSection* section = &elfSections[si];

        fs__write(handle, fileOffset, ir_section->data, ir_section->data_len);
        ELF_SET(&section->sec, sh_offset, fileOffset);
        fileOffset += ir_section->data_len;
    }


    fs__write(handle, fileOffset_sections + 0, ELF_PTR(&null_section), sectionSize);
    fs__write(handle, fileOffset_sections + sectionIndex_secstrtab, ELF_PTR(&section_strtab_section), sectionSize);
    fs__write(handle, fileOffset_sections + sectionIndex_symstrtab, ELF_PTR(&symbol_strtab_section), sectionSize);
    fs__write(handle, fileOffset_sections + sectionIndex_symtab, ELF_PTR(&symtab_section), sectionSize);
    for (int si=0;si<elfSections_len;si++) {
        ElfSection* section = &elfSections[si];
        fs__write(handle, fileOffset_sections + section->sec_index, ELF_PTR(&section->sec), sectionSize);
        fs__write(handle, fileOffset_sections + section->rela_index, ELF_PTR(&section->rela), sectionSize);
    }


    fs__write(handle, 0, ELF_PTR(&header), headerSize);

    fs__close(handle);
    
    PROFILE_END()
}

