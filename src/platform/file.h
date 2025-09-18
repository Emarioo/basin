#pragma once

#include "platform/string.h"
#include "platform/memory.h"
#include "platform/assert.h"

typedef u64 FileHandle;

FileHandle open_file(const char* path, u64* out_file_size);
void close_file(FileHandle handle);
bool read_file(FileHandle handle, char* ptr, u64 size);
bool write_file(FileHandle handle, char* ptr, u64 size);
bool get_file_info(const char* path, bool* is_dir, u64* file_size);

// string read_whole_file(const char* path);


#ifdef IMPL_PLATFORM

#ifdef OS_WINDOWS
    #include "windows.h"
#endif

FileHandle open_file(const char* path, u64* out_file_size) {
    FILE* file = fopen(path, "rb");
    if(!file) {
        if(out_file_size)
            *out_file_size = 0;
        return 0;
    }
    if(out_file_size) {
        fseek(file, 0, SEEK_END);
        *out_file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
    }
    return (FileHandle)file;
}

void close_file(FileHandle handle) {
    ASSERT(handle);
    fclose((FILE*)handle);
}

bool read_file(FileHandle handle, char* data, u64 size) {
    ASSERT(handle);
    ASSERT(size != 0);

    while(size) {
        size_t bytes = fread(data, 1, size, (FILE*)handle);
        if (bytes == 0) {
            return false;
        }
        size -= bytes;
        data += bytes;
    }

    return true;
}
bool write_file(FileHandle handle, char* data, u64 size) {
    ASSERT(handle);
    ASSERT(size != 0);

    while(size) {
        size_t bytes = fwrite(data, 1, size, (FILE*)handle);
        if (bytes == 0) {
            return false;
        }
        size -= bytes;
        data += bytes;
    }

    return true;
}

bool get_file_info(const char* path, bool* is_dir, u64* file_size) {
    if(is_dir)    *is_dir = false;
    if(file_size) *file_size = 0;

    #ifdef OS_WINDOWS
        WIN32_FILE_ATTRIBUTE_DATA data;
        WINBOOL res = GetFileAttributesExA(path, GetFileExInfoStandard, &data);
        if(!res) {
            return false;
        }

        if(is_dir)
            *is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if(!*is_dir && file_size) {
            ULARGE_INTEGER size;
            size.LowPart = data.nFileSizeLow;
            size.HighPart = data.nFileSizeHigh;
            *file_size = size.QuadPart;
        }
    #endif

    return true;
}


// bool read_whole_file(const char* path) {
//     string str = {0};
//     FILE* file = fopen(path, "rb");
//     if(!file) {
//         return str;
//     }
//     fseek(file, 0, SEEK_END);
//     int size = ftell(file);
//     fseek(file, 0, SEEK_SET);

//     void* ptr = heap_alloc(size + 1);
//     ASSERT(ptr);

//     int read = fread(ptr, 1, size, file);
//     fclose(file);
//     ASSERT(read == size);

//     str.ptr = ptr;
//     str.max = size;
//     str.len = size;
//     return str;
// }


#endif // IMPL_PLATFORM