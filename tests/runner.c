#define _GNU_SOURCE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Filesystem

typedef struct {
    char *name;
    bool is_directory;
} Platform_DirectoryEntry;

typedef struct {
    Platform_DirectoryEntry *entries;
    size_t count;
} Platform_Directory;

bool platform_list_directory(
    const char *path,
    Platform_Directory *out_directory
);

void platform_free_directory(
    Platform_Directory *directory
);

// Processes

typedef struct {
    int exit_code;
    bool success;
} Platform_ProcessResult;

Platform_ProcessResult platform_run(
    const char *program,
    const char *const *arguments
);

// Paths

bool platform_path_join(
    char *out,
    size_t capacity,
    const char *a,
    const char *b
);

bool platform_file_exists(const char *path);




#define PATH_CAPACITY 300

typedef struct {
    const char* name;
    unsigned total;
    unsigned passed;
    const char* importDir;
    const char* testutil;
    const char* objdir;
    bool verbose;
} TestSet;


static bool has_extension(const char *filename, const char *extension) {
    size_t a = strlen(filename);
    size_t b = strlen(extension);

    return a >= b && strcmp(filename + a - b, extension) == 0;
}

static void run_tests(TestSet* testSet, const char *directory) {
    Platform_Directory listing;

    if (!platform_list_directory(directory, &listing)) {
        printf("ERROR: Could not list directory: %s\n", directory);
        testSet->total++;
        return;
    }

    for (size_t i = 0; i < listing.count; i++) {
        Platform_DirectoryEntry *entry = &listing.entries[i];

        char path[PATH_CAPACITY];
        char opath[PATH_CAPACITY];
        char xpath[PATH_CAPACITY];

        if (!platform_path_join(
                path, sizeof(path), directory, entry->name)) {
            printf("ERROR: Path too long: %s\n", entry->name);
            testSet->total++;
            continue;
        }

        if (entry->is_directory) {
            run_tests(testSet, path);
            continue;
        }

        if (!has_extension(entry->name, ".bsn")) continue;

        // Pass a path relative to the tests directory when possible.
        // This keeps output readable and avoids absolute paths.

        snprintf(opath, sizeof(opath), "%s/%s.o", testSet->objdir, entry->name);
        snprintf(xpath, sizeof(xpath), "%s/%s.exe", testSet->objdir, entry->name);

        const char *arguments1[] = { "-silent", "-o", opath, path, "-I", testSet->importDir, NULL };
        const char *arguments2[] = { "-o", xpath, opath, testSet->testutil, NULL };
        const char *arguments3[] = { NULL };

        if (testSet->verbose) {
            arguments1[0] = "-z";
        }

        // printf("\nRUNNING %s\n", path);
        fflush(stdout);
        
        Platform_ProcessResult result;

        #define PLATFORM_RUN(CMD, ARGS) do {          \
            if (testSet->verbose) {                   \
                printf("%s ", CMD);                   \
                for (const char** arg = ARGS; ;arg++) {  \
                    if (!*arg)                         \
                        break;                        \
                    printf("%s ", *arg);               \
                }                                     \
                printf("\n");                         \
            }                                         \
            result = platform_run(CMD, ARGS);         \
        } while(0);

        PLATFORM_RUN("basin", arguments1);
        if (result.exit_code == 0) {
            PLATFORM_RUN("gcc", arguments2);
        }
        if (result.exit_code == 0) {
            PLATFORM_RUN(xpath, arguments3);
        }

        bool passed = result.exit_code == 0;

        if (passed) {
            printf("[PASS] %s\n", path);
            testSet->total++;
            testSet->passed++;
        } else {
            printf("[FAIL] %s", path);

            if (!result.success) {
                printf(" (could not execute)");
            } else {
                printf(" (exit code: %d)", result.exit_code);
            }

            printf("\n");
            testSet->total++;
        }
    }

    platform_free_directory(&listing);
}

int main(int argc, char **argv) {
    
    const char* importDir = NULL;
    const char* testutil = NULL;
    const char* objdir = NULL;
    bool verbose = false;

    int argi = 0;
    while (argi < argc) {
        char* arg = argv[argi];
        argi++;

        if (!strcmp(arg, "-objdir")) {
            objdir = argv[argi];
            argi++;
        } else if (!strcmp(arg, "-importdir")) {
            importDir = argv[argi];
            argi++;
        } else if (!strcmp(arg, "-testutil")) {
            testutil = argv[argi];
            argi++;
        } else if (!strcmp(arg, "-verbose")) {
            verbose = true;
        }
    }



    if (!importDir) {
        printf("ERROR: runner, -importdir was not specified\n");
        return 1;
    }
    if (!testutil) {
        printf("ERROR: runner, -testutil was not specified\n");
        return 1;
    }
    if (!objdir) {
        printf("ERROR: runner, -objdir was not specified\n");
        return 1;
    }

    TestSet testSets[] = {
        { "runtime" },
        // { "errors" },
        // { "todo" },
    };

    
    for (int i = 0; i < sizeof(testSets)/sizeof(*testSets); i++) {
        testSets->importDir = importDir;
        testSets->testutil = testutil;
        testSets->objdir = objdir;
        testSets->verbose = verbose;
    }

    printf("================================\n");
    printf("       BASIN LANGUAGE TESTS\n");
    printf("================================\n");

    unsigned total = 0;
    unsigned passed = 0;
    for (int i = 0; i < sizeof(testSets)/sizeof(*testSets); i++) {
        TestSet* testSet = &testSets[i];
        run_tests(testSet, testSet->name);
        total += testSet->total;
        passed += testSet->passed;
    }

    printf("\n================================\n");
    printf("              SUMMARY\n");
    printf("================================\n");

  for (int i = 0; i < sizeof(testSets)/sizeof(*testSets); i++) {
        TestSet* testSet = &testSets[i];
        printf("%s: %u/%u passed\n",
            testSet->name, testSet->passed, testSet->total);
    }
    
    printf("--------------------------------\n");
    printf("Total:   %u/%u passed\n",
           passed, total);

    return passed == total ? 0 : 1;
}







#include <string.h>

bool platform_path_join(
    char *out,
    size_t capacity,
    const char *a,
    const char *b
) {
    if (!out || !a || !b || capacity == 0) return false;

    size_t a_len = strlen(a);
    size_t b_len = strlen(b);

#ifdef _WIN32
    const char separator = '\\';
#else
    const char separator = '/';
#endif

    bool needs_separator =
        a_len > 0 &&
        a[a_len - 1] != '/' &&
        a[a_len - 1] != '\\';

    size_t required = a_len + b_len + (needs_separator ? 1 : 0) + 1;

    if (required > capacity) return false;

    memcpy(out, a, a_len);

    size_t offset = a_len;

    if (needs_separator) {
        out[offset++] = separator;
    }

    memcpy(out + offset, b, b_len);
    out[offset + b_len] = '\0';

    return true;
}





#ifdef __linux__


#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool directory_add(
    Platform_Directory *directory,
    const char *name,
    bool is_directory
) {
    size_t count = directory->count + 1;

    void *memory = realloc(
        directory->entries,
        count * sizeof(Platform_DirectoryEntry)
    );

    if (!memory) return false;

    directory->entries = memory;

    Platform_DirectoryEntry *entry =
        &directory->entries[directory->count];

    entry->name = strdup(name);
    if (!entry->name) return false;

    entry->is_directory = is_directory;
    directory->count = count;

    return true;
}

bool platform_list_directory(
    const char *path,
    Platform_Directory *out_directory
) {
    memset(out_directory, 0, sizeof(*out_directory));

    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *item;
    bool result = true;

    while ((item = readdir(dir))) {
        if (strcmp(item->d_name, ".") == 0 ||
            strcmp(item->d_name, "..") == 0) {
            continue;
        }

        char full_path[4096];

        if (!platform_path_join(
                full_path, sizeof(full_path), path, item->d_name)) {
            result = false;
            break;
        }

        struct stat info;

        if (stat(full_path, &info) != 0) {
            result = false;
            break;
        }

        if (!directory_add(
                out_directory,
                item->d_name,
                S_ISDIR(info.st_mode))) {
            result = false;
            break;
        }
    }

    closedir(dir);

    if (!result) {
        platform_free_directory(out_directory);
    }

    return result;
}

void platform_free_directory(Platform_Directory *directory) {
    for (size_t i = 0; i < directory->count; i++) {
        free(directory->entries[i].name);
    }

    free(directory->entries);
    memset(directory, 0, sizeof(*directory));
}

Platform_ProcessResult platform_run(
    const char *program,
    const char *const *arguments
) {
    Platform_ProcessResult result = { .exit_code = -1 };

    size_t count = 0;
    while (arguments && arguments[count]) count++;

    char **argv = calloc(count + 2, sizeof(char *));
    if (!argv) return result;

    argv[0] = (char *)program;

    for (size_t i = 0; i < count; i++) {
        argv[i + 1] = (char *)arguments[i];
    }

    pid_t pid = fork();

    if (pid == 0) {
        execvp(program, argv);
        _exit(127);
    }

    free(argv);

    if (pid < 0) return result;

    int status;
    if (waitpid(pid, &status, 0) < 0) return result;

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.success = true;
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
        result.success = true;
    }

    return result;
}

bool platform_file_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}


#endif





#ifdef _WIN32


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool directory_add(
    Platform_Directory *directory,
    const char *name,
    bool is_directory
) {
    size_t count = directory->count + 1;

    void *memory = realloc(
        directory->entries,
        count * sizeof(Platform_DirectoryEntry)
    );

    if (!memory) return false;

    directory->entries = memory;

    Platform_DirectoryEntry *entry =
        &directory->entries[directory->count];

    size_t len = strlen(name) + 1;
    entry->name = malloc(len);

    if (!entry->name) return false;

    memcpy(entry->name, name, len);
    entry->is_directory = is_directory;
    directory->count = count;

    return true;
}

bool platform_list_directory(
    const char *path,
    Platform_Directory *out_directory
) {
    memset(out_directory, 0, sizeof(*out_directory));

    char pattern[4096];

    if (!platform_path_join(
            pattern, sizeof(pattern), path, "*")) {
        return false;
    }

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) return false;

    bool result = true;

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        bool is_directory =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (!directory_add(
                out_directory, data.cFileName, is_directory)) {
            result = false;
            break;
        }

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    if (!result) {
        platform_free_directory(out_directory);
    }

    return result;
}

void platform_free_directory(Platform_Directory *directory) {
    for (size_t i = 0; i < directory->count; i++) {
        free(directory->entries[i].name);
    }

    free(directory->entries);
    memset(directory, 0, sizeof(*directory));
}

// Windows command-line quoting.

static bool append_char(
    char *buffer,
    size_t capacity,
    size_t *length,
    char c
) {
    if (*length + 1 >= capacity) return false;

    buffer[(*length)++] = c;
    buffer[*length] = '\0';
    return true;
}

static bool append_argument(
    char *buffer,
    size_t capacity,
    size_t *length,
    const char *argument
) {
    if (!append_char(buffer, capacity, length, '"')) return false;

    size_t slashes = 0;

    for (const char *p = argument;; p++) {
        char c = *p;

        if (c == '\\') {
            slashes++;
            continue;
        }

        if (c == '"' || c == '\0') {
            for (size_t i = 0; i < slashes * 2 + (c == '"'); i++) {
                if (!append_char(buffer, capacity, length, '\\'))
                    return false;
            }

            slashes = 0;

            if (c == '"') {
                if (!append_char(buffer, capacity, length, '"'))
                    return false;
                continue;
            }

            break;
        }

        while (slashes--) {
            if (!append_char(buffer, capacity, length, '\\'))
                return false;
        }

        slashes = 0;

        if (!append_char(buffer, capacity, length, c))
            return false;
    }

    return append_char(buffer, capacity, length, '"');
}

Platform_ProcessResult platform_run(
    const char *program,
    const char *const *arguments
) {
    Platform_ProcessResult result = { .exit_code = -1 };

    char command[32768] = {0};
    size_t length = 0;

    if (!append_argument(command, sizeof(command), &length, program))
        return result;

    for (size_t i = 0; arguments && arguments[i]; i++) {
        if (!append_char(command, sizeof(command), &length, ' '))
            return result;

        if (!append_argument(
                command, sizeof(command), &length, arguments[i]))
            return result;
    }

    STARTUPINFOA startup = {0};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process = {0};

    if (!CreateProcessA(
            NULL,
            command,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &startup,
            &process)) {
        return result;
    }

    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD exit_code = 0;

    if (GetExitCodeProcess(process.hProcess, &exit_code)) {
        result.exit_code = (int)exit_code;
        // result.success = true;
    }
    // printf("RES %d %d\n", result.success, exit_code);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    return result;
}

bool platform_file_exists(const char *path) {
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

#endif















