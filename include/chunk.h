#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include <stdint.h>

#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_EQ,
  OP_NOT_EQ,
  OP_GREATER,
  OP_GREATER_EQ,
  OP_LESS,
  OP_LESS_EQ,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_RETURN
} OpCode;

typedef struct chunk {
  int count;
  int capacity;
  uint8_t *code;
  int *lines; // Line number of corresponding byte in the bytecode.
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
void freeChunk(Chunk *chunk);

int addConstant(Chunk *chunk, Value value);

#endif
