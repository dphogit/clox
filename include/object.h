#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "value.h"
#include "vm.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value)  ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum obj_type { OBJ_STRING } ObjType;

struct obj {
  ObjType type;
  Obj *next;
};

struct obj_string {
  Obj obj;
  int length;
  char *chars;
};

ObjString *takeString(VM *vm, char *chars, int length);
ObjString *copyString(VM *vm, const char *chars, int length);

void printObject(Value value);

inline static bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
