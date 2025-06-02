#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct vm {
  Chunk *chunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stackTop;
  Obj *objects;
} VM;

typedef enum interpret_result {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERR,
  INTERPRET_RUNTIME_ERR
} InterpretResult;

void initVM(VM *vm);
void freeVM(VM *vm);

void pushStack(VM *vm, Value value);
Value popStack(VM *vm);

InterpretResult interpret(const char *source);

#endif
