#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include <stdint.h>

#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
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
