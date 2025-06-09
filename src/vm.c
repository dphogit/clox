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

void initVM(VM *vm) {
  resetStack(vm);

  vm->objects = NULL;
  initTable(&vm->globals);
  initTable(&vm->strings);
}

void freeVM(VM *vm) {
  freeTable(&vm->strings);
  freeTable(&vm->globals);
  freeObjects(vm);
}

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
#define READ_SHORT()    (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8 | vm->ip[-1])))
#define READ_STRING()   AS_STRING(READ_CONSTANT())

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

    // TODO: Assign global variables (OP_SET_GLOBAL)
    switch (instruction) {
      case OP_CONSTANT:  pushStack(vm, READ_CONSTANT()); break;
      case OP_NIL:       pushStack(vm, NIL_VAL); break;
      case OP_TRUE:      pushStack(vm, BOOL_VAL(true)); break;
      case OP_FALSE:     pushStack(vm, BOOL_VAL(false)); break;
      case OP_POP:       popStack(vm); break;
      case OP_GET_LOCAL: {
        uint8_t stackIndex = READ_BYTE();
        pushStack(vm, vm->stack[stackIndex]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t stackIndex    = READ_BYTE();
        vm->stack[stackIndex] = peekStack(vm, 0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value value;

        if (!tableGet(&vm->globals, name, &value)) {
          runtimeError(vm, "undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERR;
        }

        pushStack(vm, value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString *name = READ_STRING();
        tableSet(&vm->globals, name, peekStack(vm, 0));
        popStack(vm);
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();
        if (tableSet(&vm->globals, name, peekStack(vm, 0))) {
          // tableSet returning true means a new entry into the table, so
          // the variable assigning to has not be defined which is a error
          tableDelete(&vm->globals, name);
          runtimeError(vm, "undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERR;
        }
        break;
      }
      case OP_EQ: {
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
      case OP_PRINT: {
        printValue(popStack(vm));
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        vm->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsy(peekStack(vm, 0))) {
          vm->ip += offset;
        }
        break;
      }
      case OP_RETURN: return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, const char *source) {
  if (!compile(vm, source)) {
    return INTERPRET_COMPILE_ERR;
  }

  vm->ip                 = vm->chunk->code;
  InterpretResult result = run(vm);

  return result;
}
