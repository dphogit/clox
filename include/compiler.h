#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "chunk.h"

#include <stdbool.h>

/// Compilation converts the source into bytecode and writes it to the chunk.
/// Returns true if compiling succeeded, otherwise false.
bool compile(const char *source, Chunk *chunk);

#endif
