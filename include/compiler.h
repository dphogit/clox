#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "vm.h"

#include <stdbool.h>

/// Compilation converts the source into bytecode and writes it to the VM.
/// Returns true if compiling succeeded, otherwise false.
bool compile(VM *vm, const char *source);

#endif
