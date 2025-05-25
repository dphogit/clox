#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

typedef double Value;

typedef struct value_array {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *arr);
void writeValueArray(ValueArray *arr, Value value);
void freeValueArray(ValueArray *arr);

void printValue(Value value);

#endif
