#include "memory.h"
#include "object.h"
#include "vm.h"

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

void freeObject(Obj *obj) {
  switch (obj->type) {
    case OBJ_FUNCTION: {
      ObjFunction *func = (ObjFunction *)obj;
      freeChunk(&func->chunk);
      FREE(ObjFunction, obj);
      break;
    }
    case OBJ_NATIVE: {
      FREE(ObjNative, obj);
      break;
    }
    case OBJ_STRING: {
      ObjString *str = (ObjString *)obj;
      FREE_ARRAY(char, str->chars, str->length + 1);
      FREE(ObjString, obj);
      break;
    }
  }
}

void freeObjects(VM *vm) {
  Obj *obj = vm->objects;

  while (obj != NULL) {
    Obj *next = obj->next;
    freeObject(obj);
    obj = next;
  }
}
