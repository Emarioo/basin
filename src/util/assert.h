/*
    Multiple kinds of assert so we can choose which to disable.
*/

#pragma once

#define _ASSERT(expression) ((expression) ? true : (fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))
// #define _ASSERT(expression) ((expression) ? true : (FireAssertHandler(), fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))

// This assert stays in release builds.
// Use this for unreachable parts of your code.
// Do not use this in hot paths.
#define ASSERT(...) _ASSERT(__VA_ARGS__)

// Only present in debug builds.
// It's here to find bugs early during development.
#define ASSERT_DEBUG(...) _ASSERT(__VA_ARGS__)
#define ASSERT_INDEX(...) _ASSERT(__VA_ARGS__)
