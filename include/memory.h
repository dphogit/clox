#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "vm.h"

#include <stddef.h>

#define ALLOCATE(type, count) (type *)reallocate(NULL, 0, sizeof(type) * count)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, oldCap, newCap) \
  (type *)reallocate(ptr, sizeof(type) * (oldCap), sizeof(type) * (newCap))

#define FREE_ARRAY(type, ptr, oldCap) \
  reallocate(ptr, sizeof(type) * (oldCap), 0)

#define FREE(type, ptr) reallocate(ptr, sizeof(type), 0)

void *reallocate(void *ptr, size_t oldSize, size_t newSize);
void freeObjects(VM *vm);

#endif
