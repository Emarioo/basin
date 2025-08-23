/*
    Entry point and command line parsing for the compiler CLI executable
*/

#include "basin/core/lexer.h"
#include "basin/core/driver.h"

#include "platform/string.h"
#include "platform/file.h"
#include "platform/thread.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("basin <source_file>\n");
        return 1;
    }

    string source_file;
    source_file.ptr = argv[1];
    source_file.len = strlen(argv[1]);

    string text = read_file(source_file.ptr);
    if(!text.ptr) {
        printf("Cannot read '%s'\n", source_file.ptr);
        return 1;
    }

    Driver* driver = driver_create();
    
    Task task = {};
    task.kind = TASK_TOKENIZE;
    task.text = text;
    task.import_id = driver_create_import_id(driver, source_file);

    driver_add_task(driver, &task);

    driver_run(driver);

    // Driver finished compiling code
    // user metaprograms finished executing
    // Driver finished generating x86 (if that was specified), bytecode is finished at least
    // Driver spat out executable, shared library, bytecode or whatever was specified.
    
    // write it to a file


    return 0;
}