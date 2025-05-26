#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <stdlib.h>

int main() {
  VM vm;
  initVM(&vm);

  Chunk chunk;

  initChunk(&chunk);

  // -((1.2 + 3.4) / 5.6)

  int constantIndex = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constantIndex, 1);

  constantIndex = addConstant(&chunk, 3.4);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constantIndex, 1);

  writeChunk(&chunk, OP_ADD, 1);

  constantIndex = addConstant(&chunk, 5.6);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constantIndex, 1);

  writeChunk(&chunk, OP_DIVIDE, 1);

  writeChunk(&chunk, OP_NEGATE, 1);

  writeChunk(&chunk, OP_RETURN, 1);

  disassembleChunk(&chunk, "Expression: -((1.2 + 3.4) / 5.6)");
  interpret(&vm, &chunk);

  freeChunk(&chunk);
  freeVM(&vm);

  return EXIT_SUCCESS;
}
