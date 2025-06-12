#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <stdbool.h>

typedef enum value_type { VAL_BOOL, VAL_NIL, VAL_NUM, VAL_OBJ } ValueType;

typedef struct obj Obj;
typedef struct obj_string ObjString;
typedef struct obj_function ObjFunction;
typedef struct obj_native ObjNative;

typedef struct value {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj *obj;
  } as;
} Value;

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL         ((Value){VAL_NIL, {.number = 0}})
#define NUM_VAL(value)  ((Value){VAL_NUM, {.number = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj *)object}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value)  ((value).type == VAL_NIL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value)  ((value).as.number)
#define AS_OBJ(value)  ((value).as.obj)

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
