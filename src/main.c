#include "vm.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void repl() {
  char *line = NULL;
  size_t len = 0;
  ssize_t nread;

  while (true) {
    printf("clox> ");
    if ((nread = getline(&line, &len, stdin)) == -1) {
      break;
    }

    if (line[nread - 1] == '\n') {
      line[nread - 1] = '\0';
    }

    interpret(line);
  }

  free(line);
}

static char *readFile(const char *path) {
  FILE *file;
  long fileSizeBytes;
  char *source;
  size_t bytesRead;

  if ((file = fopen(path, "r")) == NULL) {
    fprintf(stderr, "Could not open file '%s'", path);
    exit(EXIT_FAILURE);
  }

  if (fseek(file, 0, SEEK_END) == -1) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }

  if ((fileSizeBytes = ftell(file)) == -1) {
    perror("ftell");
    exit(EXIT_FAILURE);
  }

  if (fseek(file, 0, SEEK_SET) == -1) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }

  if ((source = malloc(fileSizeBytes + 1)) == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  bytesRead = fread(source, sizeof(char), fileSizeBytes, file);
  if (bytesRead != (unsigned long)fileSizeBytes) {
    fprintf(stderr, "Could not open file '%s'", path);
    exit(EXIT_FAILURE);
  }

  source[fileSizeBytes] = '\0';

  fclose(file);

  return source;
}

void runFile(const char *path) {
  char *source = readFile(path);

  InterpretResult result = interpret(source);

  free(source);

  if (result != INTERPRET_OK)
    exit(EXIT_FAILURE);
}

void usage() { printf("Usage: clox [path]\n"); }

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    usage();
    exit(2);
  }

  return EXIT_SUCCESS;
}
