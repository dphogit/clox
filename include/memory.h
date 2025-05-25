#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include <stddef.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, oldCap, newCap) \
  (type *)reallocate(ptr, sizeof(type) * (oldCap), sizeof(type) * (newCap))

#define FREE_ARRAY(type, ptr, oldCap) \
  reallocate(ptr, sizeof(type) * (oldCap), 0)

void *reallocate(void *ptr, size_t oldSize, size_t newSize);

#endif
