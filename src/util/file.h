#pragma once

#include "util/string.h"

string util_read_whole_file(const char* path);

bool util_write_whole_file(const char* path, const cstring text);
