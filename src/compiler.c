#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "scanner.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct parser {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
  Scanner *scanner;
  Chunk *chunk; // The chunk that the bytecode will be written to.
} Parser;

// The language's precdence levels from lowest to highest
typedef enum precedence {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser *parser);

// A parse rule encapsulates three properties for a token type:
// 1. The fn to compile a prefix expression starting with the token type
// 2. The fn to compile an infix expression whose left operand is followed
//    by the token type
// 3. Precedence of an infix expression that uses that token as an operator.
// The precedence of a prefix expression starting with a token is not required
// because all prefix operators in this language have the same precedence.
typedef struct parse_rule {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void errorAt(Parser *parser, Token *token, const char *message) {
  // Supress cascaded errors
  if (parser->panicMode)
    return;

  parser->panicMode = true;

  fprintf(stderr, "[line %d], Error", token->line);

  if (token->type == TOK_EOF) {
    fprintf(stderr, "at end");
  } else if (token->type == TOK_ERR) {
    // TODO: Handle TOK_ERR
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser->hadError = true;
}

static void errorAtPrevious(Parser *parser, const char *message) {
  errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser *parser, const char *message) {
  errorAt(parser, &parser->current, message);
}

static void advance(Parser *parser) {
  parser->previous = parser->current;

  while (true) {
    parser->current = scanToken(parser->scanner);

    if (parser->current.type != TOK_ERR)
      break;

    errorAtCurrent(parser, parser->current.start);
  }
}

static void consume(Parser *parser, TokenType type, const char *message) {
  if (parser->current.type != type) {
    errorAtCurrent(parser, message);
    return;
  }

  advance(parser);
}

static void emitByte(Parser *parser, uint8_t byte) {
  writeChunk(parser->chunk, byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
  writeChunk(parser->chunk, byte1, parser->previous.line);
  writeChunk(parser->chunk, byte2, parser->previous.line);
}

static void emitReturn(Parser *parser) { emitByte(parser, OP_RETURN); }

static void endCompiler(Parser *parser) {
  emitReturn(parser);

#ifdef DEBUG_PRINT_CODE
  disassembleChunk(parser->chunk, "code");
#endif
}

static int makeConstant(Parser *parser, Value value) {
  if (parser->chunk->constants.count >= UINT8_MAX) {
    // A byte for the index means we can only store 256 constants in a chunk.
    errorAtPrevious(parser, "Too many constants in one chunk.");
    return -1;
  }

  int constantIndex = addConstant(parser->chunk, value);
  return constantIndex;
}

static void emitConstant(Parser *parser, Value value) {
  int constantIndex = makeConstant(parser, value);
  if (constantIndex != -1) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
  }
}

static void expression(Parser *parser);
static void parsePrecedence(Parser *parser, Precedence precedence);
static ParseRule *getRule(TokenType type);

static void number(Parser *parser) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, value);
}

static void grouping(Parser *parser) {
  expression(parser);
  consume(parser, TOK_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(Parser *parser) {
  TokenType opType = parser->previous.type;

  // Compile operand
  parsePrecedence(parser, PREC_UNARY);

  // Emit operator instruction
  switch (opType) {
    case TOK_MINUS: emitByte(parser, OP_NEGATE);
    default:        return;
  }
}

static void binary(Parser *parser) {
  TokenType opType = parser->previous.type;

  // Compile right hand operand.
  // Use 1 higher level of precedence for the right operand because
  // binary operators are left associative.
  // e.g. 1 + 2 + 3 + 4 => ((1 + 2) + 3) + 4
  ParseRule *rule = getRule(opType);
  parsePrecedence(parser, rule->precedence + 1);

  switch (opType) {
    case TOK_PLUS:  emitByte(parser, OP_ADD); break;
    case TOK_MINUS: emitByte(parser, OP_SUBTRACT); break;
    case TOK_STAR:  emitByte(parser, OP_MULTIPLY); break;
    case TOK_SLASH: emitByte(parser, OP_DIVIDE); break;
    default:        return;
  }
}

// The table of parse rules that drives the parser.
ParseRule rules[] = {
    [TOK_LEFT_PAREN]  = {grouping, NULL,   PREC_NONE  },
    [TOK_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE  },
    [TOK_LEFT_BRACE]  = {NULL,     NULL,   PREC_NONE  },
    [TOK_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE  },
    [TOK_COMMA]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_DOT]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_MINUS]       = {unary,    binary, PREC_TERM  },
    [TOK_PLUS]        = {NULL,     binary, PREC_TERM  },
    [TOK_SEMICOLON]   = {NULL,     NULL,   PREC_NONE  },
    [TOK_SLASH]       = {NULL,     binary, PREC_FACTOR},
    [TOK_STAR]        = {NULL,     binary, PREC_FACTOR},
    [TOK_BANG]        = {NULL,     NULL,   PREC_NONE  },
    [TOK_BANG_EQ]     = {NULL,     NULL,   PREC_NONE  },
    [TOK_EQ]          = {NULL,     NULL,   PREC_NONE  },
    [TOK_EQ_EQ]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_GREATER]     = {NULL,     NULL,   PREC_NONE  },
    [TOK_GREATER_EQ]  = {NULL,     NULL,   PREC_NONE  },
    [TOK_LESS]        = {NULL,     NULL,   PREC_NONE  },
    [TOK_LESS_EQ]     = {NULL,     NULL,   PREC_NONE  },
    [TOK_IDENTIFIER]  = {NULL,     NULL,   PREC_NONE  },
    [TOK_STRING]      = {NULL,     NULL,   PREC_NONE  },
    [TOK_NUMBER]      = {number,   NULL,   PREC_NONE  },
    [TOK_AND]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_CLASS]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_ELSE]        = {NULL,     NULL,   PREC_NONE  },
    [TOK_FALSE]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_FOR]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_FUN]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_IF]          = {NULL,     NULL,   PREC_NONE  },
    [TOK_NIL]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_OR]          = {NULL,     NULL,   PREC_NONE  },
    [TOK_PRINT]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_RETURN]      = {NULL,     NULL,   PREC_NONE  },
    [TOK_SUPER]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_THIS]        = {NULL,     NULL,   PREC_NONE  },
    [TOK_TRUE]        = {NULL,     NULL,   PREC_NONE  },
    [TOK_VAR]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_WHILE]       = {NULL,     NULL,   PREC_NONE  },
    [TOK_ERR]         = {NULL,     NULL,   PREC_NONE  },
    [TOK_EOF]         = {NULL,     NULL,   PREC_NONE  },
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

// At the current token, parses any expression at the given level or higher.
static void parsePrecedence(Parser *parser, Precedence precedence) {
  /*
   * Example: "1 + 2"
   * - expression(PREC_ASSIGNMENT)
   * - rules[TOK_NUMBER]->prefix => number() => emit "1"
   * - currently on "+" token (PREC_TERM). Enter loop as ASSIGNMENT <= TERM.
   *   a. advance the parser (consuming "+")
   *   b. rules[TOK_PLUS]->infix => binary => emit "2" (number()) and "+"
   *   c. current token "2" has PREC_NONE. ASSIGNMENT > NONE so exit loop.
   */

  advance(parser);

  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    errorAtPrevious(parser, "Expect expression.");
    return;
  }

  prefixRule(parser);

  // Compile infix expression
  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(parser);
  }
}

static void expression(Parser *parser) {
  parsePrecedence(parser, PREC_ASSIGNMENT);
}

bool compile(const char *source, Chunk *chunk) {
  Scanner scanner;
  initScanner(&scanner, source);

  Parser parser;
  parser.hadError  = false;
  parser.panicMode = false;
  parser.scanner   = &scanner;
  parser.chunk     = chunk;

  advance(&parser);
  expression(&parser);
  consume(&parser, TOK_EOF, "Expect end of expression.");
  endCompiler(&parser);

  return !parser.hadError;
}
