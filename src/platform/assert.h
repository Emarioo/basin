/*
    Multiple kinds of assert so we can choose which to disable.
*/

#pragma once

#define ASSERT(...) assert(__VA_ARGS__)

#define ASSERT_INDEX(...) assert(__VA_ARGS__)