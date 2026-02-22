#pragma once

#include "basin/types.h"

#include "basin/backend/ir.h"

typedef struct Driver Driver;
typedef struct AST AST;

Result generate_ir(Driver* driver, AST* ast, IRProgram* program);
