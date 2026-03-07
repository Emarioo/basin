#pragma once

extern bool enabled_logging_driver;

void dump_hex(void* address, int size, int stride);

#define debug(...) fprintf(stderr, __VA_ARGS__)
// #define debug(...)

#define should_debug_print() true
// #define should_debug_print() false
