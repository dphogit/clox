#include "memory.h"

#include <stdlib.h>

void *reallocate(void *ptr, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(ptr);
    return NULL;
  }

  void *result = realloc(ptr, newSize);

  if (result == NULL)
    exit(EXIT_FAILURE);

  return result;
}
