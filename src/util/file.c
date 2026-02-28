#include "util/file.h"

#include "platform/platform.h"

string util_read_whole_file(const char* path) {
    TracyCZone(zone, 1);
    
    FSHandle handle = fs__open(path, FS_READ);
    if(handle == FS_INVALID_HANDLE) {
        goto end;
    }

    FSInfo info;
    fs__info(handle, &info);

    string out = string_create(info.file_size + 1);
    
    bool success = fs__read(handle, 0, out.ptr, info.file_size);
    if(!success) {
        goto end;
    }

    out.len += info.file_size;
    out.ptr[out.len] = '\0';

    fs__close(handle);

end:
    TracyCZoneEnd(zone);
    return out;
}

bool util_write_whole_file(const char* path, const cstring text) {
    TracyCZone(zone, 1);

    bool res = false;

    FSHandle handle = fs__open(path, FS_READ);
    if(handle == FS_INVALID_HANDLE) {
        goto end;
    }

    bool success = fs__write(handle, 0, text.ptr, text.len);
    if(!success) {
        fs__close(handle);
        goto end;
    }

    fs__close(handle);
    res = true;

end:
    TracyCZoneEnd(zone);
    return res;
}