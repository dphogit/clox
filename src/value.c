#include "value.h"
#include "memory.h"

#include <stddef.h>
#include <stdio.h>

void initValueArray(ValueArray *arr) {
  arr->values = NULL;
  arr->count = 0;
  arr->capacity = 0;
}

void writeValueArray(ValueArray *arr, Value value) {
  if (arr->count >= arr->capacity) {
    int oldCap = arr->capacity;
    arr->capacity = GROW_CAPACITY(oldCap);
    arr->values = GROW_ARRAY(Value, arr->values, oldCap, arr->capacity);
  }

  arr->values[arr->count++] = value;
}

void freeValueArray(ValueArray *arr) {
  FREE_ARRAY(Value, arr->values, arr->capacity);
  initValueArray(arr);
}

void printValue(Value value) { printf("%g", value); }
