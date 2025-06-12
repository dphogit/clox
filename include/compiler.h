#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "vm.h"

#include <stdbool.h>

ObjFunction *compile(VM *vm, const char *source);

#endif
