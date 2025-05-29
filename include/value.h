#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "stdbool.h"

typedef enum value_type { VAL_BOOL, VAL_NIL, VAL_NUM } ValueType;

typedef struct value {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL         ((Value){VAL_NIL, {.number = 0}})
#define NUM_VAL(value)  ((Value){VAL_NUM, {.number = value}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value)  ((value).type == VAL_NIL)
#define IS_NUM(value)  ((value).type == VAL_NUM)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value)  ((value).as.number)

typedef struct value_array {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *arr);
void writeValueArray(ValueArray *arr, Value value);
void freeValueArray(ValueArray *arr);

bool valuesEqual(Value a, Value b);
void printValue(Value value);

#endif
