#pragma once

/*
    This file is forcefully included everywhere
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tracy/TracyC.h"

// @TODO Move these elsewhere.
//   common.h perhaps?
#include "util/assert.h"
#include "basin/logger.h"

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
