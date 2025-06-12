#include "object.h"
#include "memory.h"
#include "value.h"
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

static ObjString *allocateString(VM *vm, char *chars, int length,
                                 uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
  string->chars     = chars;
  string->length    = length;
  string->hash      = hash;

  tableSet(&vm->strings, string, NIL_VAL);

  return string;
}

// Hashes the string using 32-bit FNV-1a
static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

ObjFunction *newFunction(VM *vm) {
  ObjFunction *func = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
  func->arity       = 0;
  func->name        = NULL;
  initChunk(&func->chunk);
  return func;
}

ObjNative *newNative(VM *vm, NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
  native->function  = function;
  return native;
}

// Takes ownership of an existing dynamically allocated string.
// If interned, we free the passed one and return the existing reference.
ObjString *takeString(VM *vm, char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, (void *)chars, length + 1);
    return interned;
  }

  return allocateString(vm, chars, length, hash);
}

// Conservatively allocates memory for a new copy of the string if not already
// interned which we return the existing reference
ObjString *copyString(VM *vm, const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL)
    return interned;

  char *buffer = ALLOCATE(char, length + 1);
  strncpy(buffer, chars, length);
  buffer[length] = '\0';

  return allocateString(vm, buffer, length, hash);
}

static void printFunction(ObjFunction *func) {
  if (func->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", func->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_FUNCTION: printFunction(AS_FUNCTION(value)); break;
    case OBJ_NATIVE:   printf("<native fn>"); break;
    case OBJ_STRING:   printf("%s", AS_CSTRING(value)); break;
  }
}
