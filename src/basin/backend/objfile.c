#include "basin/backend/objfile.h"

#include "basin/basin.h"

void generate_object_file(Compilation* compilation) {
    printf("Gen object %s\n", compilation->options->output_file);
}
