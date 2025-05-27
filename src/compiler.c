#include "compiler.h"
#include "scanner.h"

#include <stdbool.h>
#include <stdio.h>

void compile(const char *source) {
  Scanner scanner;

  initScanner(&scanner, source);

  int line = -1;

  while (true) {
    Token token = scanToken(&scanner);

    if (token.line == line) {
      printf("   | ");
    } else {
      printf("%4d ", token.line);
      line = token.line;
    }

    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOK_EOF)
      break;
  }
}
