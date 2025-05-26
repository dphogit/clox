#include "vm.h"
#include "chunk.h"
#include "debug.h"

#include <stdbool.h>
#include <stdio.h>

static void resetStack(VM *vm) { vm->stackTop = vm->stack; }

void initVM(VM *vm) { resetStack(vm); }

void freeVM(VM *vm) {}

void pushStack(VM *vm, Value value) {
  *vm->stackTop = value;
  vm->stackTop++;
}

Value popStack(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op)                          \
  do {                                         \
    double b = popStack(vm), a = popStack(vm); \
    pushStack(vm, a op b);                     \
  } while (false);

  while (true) {
#ifdef DEBUG_TRACE_EXEC
    // Print the stack, with the right as the top of the stack.
    printf("          ");
    if (vm->stackTop - vm->stack == 0) {
      printf("<Empty>");
    } else {
      for (Value *element = vm->stack; element < vm->stackTop; element++) {
        printf("[ ");
        printValue(*element);
        printf(" ]");
      }
    }
    printf("\n");

    // Print the disassembly of the the bytecode instruction
    uint8_t offset = vm->ip - vm->chunk->code;
    disassembleInstruction(vm->chunk, offset);
#endif

    uint8_t instruction = READ_BYTE();

    switch (instruction) {
      case OP_CONSTANT: pushStack(vm, READ_CONSTANT()); break;
      case OP_ADD:      BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE:   BINARY_OP(/); break;
      case OP_NEGATE:   *(vm->stackTop - 1) = -(*(vm->stackTop - 1)); break;
      case OP_RETURN:
        printValue(popStack(vm));
        printf("\n");
        return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = chunk->code;
  return run(vm);
}
