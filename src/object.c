#include "object.h"
#include "memory.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(vm, type, objType) \
  (type *)allocateObj(vm, sizeof(type), objType)

static Obj *allocateObj(VM *vm, size_t size, ObjType type) {
  Obj *obj  = (Obj *)reallocate(NULL, 0, size);
  obj->type = type;

  // Prepend new object to VM's tracked linked list of objects
  obj->next   = vm->objects;
  vm->objects = obj;

  return obj;
}

static ObjString *allocateString(VM *vm, char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
  string->chars     = chars;
  string->length    = length;
  return string;
}

ObjString *takeString(VM *vm, char *chars, int length) {
  return allocateString(vm, chars, length);
}

ObjString *copyString(VM *vm, const char *chars, int length) {
  char *buffer = ALLOCATE(char, length + 1);
  strncpy(buffer, chars, length);
  buffer[length] = '\0';
  return allocateString(vm, buffer, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
  }
}
