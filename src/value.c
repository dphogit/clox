#include "value.h"
#include "memory.h"
#include "object.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

void initValueArray(ValueArray *arr) {
  arr->values   = NULL;
  arr->count    = 0;
  arr->capacity = 0;
}

void writeValueArray(ValueArray *arr, Value value) {
  if (arr->count >= arr->capacity) {
    int oldCap    = arr->capacity;
    arr->capacity = GROW_CAPACITY(oldCap);
    arr->values   = GROW_ARRAY(Value, arr->values, oldCap, arr->capacity);
  }

  arr->values[arr->count++] = value;
}

void freeValueArray(ValueArray *arr) {
  FREE_ARRAY(Value, arr->values, arr->capacity);
  initValueArray(arr);
}

static bool objValsEqual(Value a, Value b) {
  ObjString *aStr = AS_STRING(a), *bStr = AS_STRING(b);
  return aStr->length == bStr->length &&
         strncmp(aStr->chars, bStr->chars, aStr->length) == 0;
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;

  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:  return true;
    case VAL_NUM:  return AS_NUM(a) == AS_NUM(b);
    case VAL_OBJ:  return objValsEqual(a, b);
    default:       return false;
  }
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NIL:  printf("nil"); break;
    case VAL_NUM:  printf("%g", AS_NUM(value)); break;
    case VAL_OBJ:  printObject(value); break;
  }
}
