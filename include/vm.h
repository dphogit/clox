#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX  (FRAMES_MAX * 256)

// Represents a single ongoing function call.
typedef struct call_frame {
  ObjFunction *function;
  uint8_t *ip;  // IP the VM jumps to returning from a function
  Value *slots; // Points into the first slot used in the VMs stack
} CallFrame;

typedef struct vm {
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  Value stack[STACK_MAX];
  Value *stackTop;
  Table strings; // String interning table (hashset)
  Table globals; // Global variables
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

InterpretResult interpret(VM *vm, const char *source);

#endif
