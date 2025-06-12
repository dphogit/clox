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
#include <time.h>

static void resetStack(VM *vm) {
  vm->stackTop   = vm->stack;
  vm->frameCount = 0;
}

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Print the stack trace of the error, starting from previous failure
  for (int i = vm->frameCount - 1; i >= 0; i--) {
    CallFrame *frame  = &vm->frames[i];
    ObjFunction *func = frame->function;
    size_t lineIndex  = frame->ip - frame->function->chunk.code - 1;
    int line          = frame->function->chunk.lines[lineIndex];

    fprintf(stderr, "[line %d] in ", line);
    if (func->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", func->name->chars);
    }
  }

  resetStack(vm);
}

static void defineNative(VM *vm, const char *name, NativeFn function) {
  // We need to push and pop the name and function on the stack because
  // copyString and newNative dynamically allocate memory, triggering GC.
  pushStack(vm, OBJ_VAL(copyString(vm, name, strlen(name))));
  pushStack(vm, OBJ_VAL(newNative(vm, function)));
  tableSet(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  popStack(vm);
  popStack(vm);
}

static Value clockNative(int __attribute__((unused)) argCount,
                         Value __attribute__((unused)) * args) {
  return NUM_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNativeFunctions(VM *vm) {
  defineNative(vm, "clock", clockNative);
}

void initVM(VM *vm) {
  resetStack(vm);

  vm->objects = NULL;
  initTable(&vm->globals);
  initTable(&vm->strings);

  defineNativeFunctions(vm);
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

static bool call(VM *vm, ObjFunction *func, int argCount) {
  if (argCount != func->arity) {
    runtimeError(vm, "expected %d arguments, but got %d", argCount,
                 func->arity);
    return false;
  }

  if (vm->frameCount == FRAMES_MAX) {
    runtimeError(vm, "stack overflow");
    return false;
  }

  CallFrame *frame = &vm->frames[vm->frameCount++];
  frame->function  = func;
  frame->ip        = func->chunk.code;
  frame->slots     = vm->stackTop - argCount - 1;
  return true;
}

static bool callValue(VM *vm, Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_FUNCTION: return call(vm, AS_FUNCTION(callee), argCount);
      case OBJ_NATIVE:   {
        NativeFn native = AS_NATIVE(callee);
        Value result    = native(argCount, vm->stackTop - argCount);
        vm->stackTop -= argCount + 1;
        pushStack(vm, result);
        return true;
      }
      default: break;
    }
  }

  runtimeError(vm, "can only call functions and classes");
  return false;
}

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
  CallFrame *frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

#define READ_SHORT() \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8 | frame->ip[-1])))

#define READ_STRING() AS_STRING(READ_CONSTANT())

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

    int offset = frame->ip - frame->function->chunk.code;
    disassembleInstruction(&frame->function->chunk, offset);
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
        uint8_t slotIndex = READ_BYTE();
        pushStack(vm, frame->slots[slotIndex]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slotIndex       = READ_BYTE();
        frame->slots[slotIndex] = peekStack(vm, 0);
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
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsy(peekStack(vm, 0))) {
          frame->ip += offset;
        }
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(vm, peekStack(vm, argCount), argCount))
          return INTERPRET_RUNTIME_ERR;

        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
      case OP_RETURN: {
        Value result = popStack(vm);
        vm->frameCount--;

        // If we just discarded the very last call frame, pop "main" function
        // and the entire program is done and exit the interpreter
        if (vm->frameCount == 0) {
          popStack(vm);
          return INTERPRET_OK;
        }

        // Discard slots callee was using for its paraemters and local variables
        // Top of stack ends at beginning of the returning function stack window
        // and push the return value onto the stack
        vm->stackTop = frame->slots;
        pushStack(vm, result);
        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, const char *source) {
  // Successful compilation gives compiled top-level code.
  // This will be the "main" function call frame, with it being at VM slot 0.
  ObjFunction *func = compile(vm, source);
  if (func == NULL)
    return INTERPRET_COMPILE_ERR;

  pushStack(vm, OBJ_VAL(func));
  call(vm, func, 0);

  return run(vm);
}
