#pragma once

#include "basin/types.h"

typedef struct Driver Driver;
typedef struct AST AST;
typedef struct IRCollection IRCollection;

Result generate_ir(Driver* driver, AST* ast, IRCollection* collection);
