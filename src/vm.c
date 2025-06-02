#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void resetStack(VM *vm) { vm->stackTop = vm->stack; }

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Get the previous instruction's corresponding line for the error
  size_t lineIndex = vm->ip - vm->chunk->code - 1;
  int line         = vm->chunk->lines[lineIndex];
  fprintf(stderr, "[line %d] in script\n", line);

  resetStack(vm);
}

void initVM(VM *vm) { resetStack(vm); }

void freeVM(VM *vm) { freeObjects(vm); }

void pushStack(VM *vm, Value value) {
  *vm->stackTop = value;
  vm->stackTop++;
}

Value popStack(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

Value peekStack(VM *vm, int dist) { return vm->stackTop[-(dist + 1)]; }

static bool isFalsy(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM *vm) {
  ObjString *b = AS_STRING(popStack(vm)), *a = AS_STRING(popStack(vm));

  int n        = a->length + b->length;
  char *buffer = ALLOCATE(char, n + 1);
  memcpy(buffer, a->chars, a->length);
  memcpy(buffer + a->length, b->chars, b->length);
  buffer[n] = '\0';

  ObjString *res = takeString(vm, buffer, n);
  pushStack(vm, OBJ_VAL(res));
}

static InterpretResult run(VM *vm) {
#define READ_BYTE()     (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

#define BINARY_OP(valueType, op)                                  \
  do {                                                            \
    if (!IS_NUM(peekStack(vm, 0)) || !IS_NUM(peekStack(vm, 1))) { \
      runtimeError(vm, "Operands must be numbers.");              \
      return INTERPRET_RUNTIME_ERR;                               \
    }                                                             \
    double b = AS_NUM(popStack(vm)), a = AS_NUM(popStack(vm));    \
    pushStack(vm, valueType(a op b));                             \
  } while (false);

#ifdef DEBUG_TRACE_EXEC
  printf("== Trace Exec ==\n");
#endif

  while (true) {
#ifdef DEBUG_TRACE_EXEC

    printf("          ");
    if (vm->stackTop - vm->stack == 0) {
      printf("[empty]");
    } else {
      for (Value *valuePtr = vm->stack; valuePtr < vm->stackTop; valuePtr++) {
        printf("[ ");
        printValue(*valuePtr);
        printf(" ]");
      }
    }
    printf("\n");

    int offset = vm->ip - vm->chunk->code;
    disassembleInstruction(vm->chunk, offset);
#endif

    uint8_t instruction = READ_BYTE();

    switch (instruction) {
      case OP_CONSTANT: pushStack(vm, READ_CONSTANT()); break;
      case OP_NIL:      pushStack(vm, NIL_VAL); break;
      case OP_TRUE:     pushStack(vm, BOOL_VAL(true)); break;
      case OP_FALSE:    pushStack(vm, BOOL_VAL(false)); break;
      case OP_EQ:       {
        Value b = popStack(vm), a = popStack(vm);
        pushStack(vm, BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_NOT_EQ: {
        Value b = popStack(vm), a = popStack(vm);
        pushStack(vm, BOOL_VAL(!valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
      case OP_GREATER_EQ: BINARY_OP(BOOL_VAL, >=); break;
      case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
      case OP_LESS_EQ:    BINARY_OP(BOOL_VAL, <=); break;
      case OP_ADD:        {
        Value p0 = peekStack(vm, 0), p1 = peekStack(vm, 1);
        if (IS_STRING(p0) && IS_STRING(p1)) {
          concatenate(vm);
        } else if (IS_NUM(p0) && IS_NUM(p1)) {
          double b = AS_NUM(popStack(vm)), a = AS_NUM(popStack(vm));
          pushStack(vm, NUM_VAL(a + b));
        } else {
          runtimeError(vm, "operands must both be numbers or both be strings");
          return INTERPRET_RUNTIME_ERR;
        }

        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUM_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUM_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUM_VAL, /); break;
      case OP_NOT:
        *(vm->stackTop - 1) = BOOL_VAL(isFalsy(peekStack(vm, 0)));
        break;
      case OP_NEGATE:
        if (!IS_NUM(peekStack(vm, 0))) {
          runtimeError(vm, "operand must be a number");
          return INTERPRET_RUNTIME_ERR;
        }

        *(vm->stackTop - 1) = NUM_VAL(-AS_NUM(peekStack(vm, 0)));
        break;
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

InterpretResult interpret(const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  VM vm;
  initVM(&vm);
  vm.chunk = &chunk;

  if (!compile(&vm, source)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERR;
  }

  vm.ip                  = chunk.code;
  InterpretResult result = run(&vm);

  freeChunk(&chunk);
  return result;
}
