#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "vm.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)   isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)   isObjType(value, OBJ_STRING)

#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value)   (((ObjNative *)AS_OBJ(value))->function)
#define AS_STRING(value)   ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)  (((ObjString *)AS_OBJ(value))->chars)

typedef enum obj_type { OBJ_FUNCTION, OBJ_NATIVE, OBJ_STRING } ObjType;

struct obj {
  ObjType type;
  Obj *next;
};

struct obj_function {
  Obj obj;
  int arity;
  Chunk chunk;
  ObjString *name; // User defined functions have names
};

typedef Value (*NativeFn)(int argCount, Value *args);

struct obj_native {
  Obj obj;
  NativeFn function;
};

struct obj_string {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

ObjFunction *newFunction(VM *vm);

ObjNative *newNative(VM *vm, NativeFn function);

ObjString *takeString(VM *vm, char *chars, int length);
ObjString *copyString(VM *vm, const char *chars, int length);

void printObject(Value value);

inline static bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
