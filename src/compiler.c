#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct local {
  Token name;
  int depth;
} Local;

typedef struct compiler {
  Local locals[UINT8_MAX + 1];
  int localCount;
  int scopeDepth;
} Compiler;

typedef struct parser {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
  Scanner *scanner;
  Compiler *currentCompiler;
  VM *vm;
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

typedef void (*ParseFn)(Parser *parser, bool canAssign);

/*
 * A parse rule encapsulates three properties for a token type:
 * 1. The fn to compile a prefix expression starting with the token type
 * 2. The fn to compile an infix expression whose left operand is followed
 * by the token type
 * 3. Precedence of an infix expression that uses that token as an operator.
 * The precedence of a prefix expression starting with a token is not required
 * because all prefix operators in this language have the same precedence.
 */
typedef struct parse_rule {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
}

static void errorAt(Parser *parser, Token *token, const char *message) {
  // Supress cascaded errors
  if (parser->panicMode)
    return;

  parser->panicMode = true;

  fprintf(stderr, "[line %d], Error", token->line);

  if (token->type == TOK_EOF) {
    fprintf(stderr, " at end");
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

static bool check(Parser *parser, TokenType type) {
  return parser->current.type == type;
}

// Advances the parser if its current token matches the given type, returning
// true if so. Otherwise, returns false.
static bool match(Parser *parser, TokenType type) {
  if (!check(parser, type))
    return false;

  advance(parser);
  return true;
}

static void emitByte(Parser *parser, uint8_t byte) {
  writeChunk(parser->vm->chunk, byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
  writeChunk(parser->vm->chunk, byte1, parser->previous.line);
  writeChunk(parser->vm->chunk, byte2, parser->previous.line);
}

static void emitReturn(Parser *parser) { emitByte(parser, OP_RETURN); }

static void endCompiler(Parser *parser) {
  emitReturn(parser);

#ifdef DEBUG_PRINT_CODE
  disassembleChunk(parser->vm->chunk, "code");
#endif
}

static void beginScope(Parser *parser) {
  parser->currentCompiler->scopeDepth++;
}

static void endScope(Parser *parser) {
  Compiler *compiler = parser->currentCompiler;

  compiler->scopeDepth--;

  // Discard the local variables belonging to this scope
  while (compiler->localCount > 0 &&
         compiler->locals[compiler->localCount - 1].depth >
             compiler->scopeDepth) {
    emitByte(parser, OP_POP);
    compiler->localCount--;
  }
}

static bool inLocalScope(Parser *parser) {
  return parser->currentCompiler->scopeDepth > 0;
}

static bool inGlobalScope(Parser *parser) {
  return parser->currentCompiler->scopeDepth == 0;
}

static int makeConstant(Parser *parser, Value value) {
  if (parser->vm->chunk->constants.count >= UINT8_MAX) {
    // A byte for the index means we can only store 256 constants in a chunk.
    errorAtPrevious(parser, "Too many constants in one chunk.");
    return -1;
  }

  int constantIndex = addConstant(parser->vm->chunk, value);
  return constantIndex;
}

static int identifierConstant(Parser *parser, Token *name) {
  Value nameVal = OBJ_VAL(copyString(parser->vm, name->start, name->length));
  return makeConstant(parser, nameVal);
}

static bool identifiersEqual(Token *a, Token *b) {
  return a->length == b->length && strncmp(a->start, b->start, a->length) == 0;
}

static void emitConstant(Parser *parser, Value value) {
  int constantIndex = makeConstant(parser, value);
  if (constantIndex != -1) {
    emitBytes(parser, OP_CONSTANT, constantIndex);
  }
}

static void addLocal(Parser *parser, Token name) {
  Compiler *compiler = parser->currentCompiler;

  // The operand index to a local variable instruction is limited to a byte
  if (compiler->localCount > UINT8_MAX) {
    errorAtPrevious(parser, "Too many local variables in function.");
    return;
  }

  Local *local = &compiler->locals[compiler->localCount++];
  local->name  = name;
  local->depth = -1;
}

// Walk the array of locals backwards to find the last declared variable with
// the given identifier/name. Returns the index found, otherwise -1.
static int resolveLocal(Parser *parser, Token *name) {
  Compiler *compiler = parser->currentCompiler;

  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];

    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        errorAtPrevious(parser,
                        "Can't read local variable int its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

static void expression(Parser *parser);
static void statement(Parser *parser);
static void declarationStatement(Parser *parser);
static void parsePrecedence(Parser *parser, Precedence precedence);
static ParseRule *getRule(TokenType type);

static void number(Parser *parser, bool __attribute__((unused)) canAssign) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, NUM_VAL(value));
}

static void grouping(Parser *parser, bool __attribute__((unused)) canAssign) {
  expression(parser);
  consume(parser, TOK_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(Parser *parser, bool __attribute__((unused)) canAssign) {
  TokenType opType = parser->previous.type;

  // Compile operand
  parsePrecedence(parser, PREC_UNARY);

  // Emit operator instruction
  switch (opType) {
    case TOK_MINUS: emitByte(parser, OP_NEGATE); break;
    case TOK_BANG:  emitByte(parser, OP_NOT); break;
    default:        return;
  }
}

static void binary(Parser *parser, bool __attribute__((unused)) canAssign) {
  TokenType opType = parser->previous.type;

  // Compile right hand operand.
  // Use 1 higher level of precedence for the right operand because
  // binary operators are left associative.
  // e.g. 1 + 2 + 3 + 4 => ((1 + 2) + 3) + 4
  ParseRule *rule = getRule(opType);
  parsePrecedence(parser, rule->precedence + 1);

  switch (opType) {
    case TOK_PLUS:       emitByte(parser, OP_ADD); break;
    case TOK_MINUS:      emitByte(parser, OP_SUBTRACT); break;
    case TOK_STAR:       emitByte(parser, OP_MULTIPLY); break;
    case TOK_SLASH:      emitByte(parser, OP_DIVIDE); break;
    case TOK_BANG_EQ:    emitByte(parser, OP_NOT_EQ); break;
    case TOK_GREATER:    emitByte(parser, OP_GREATER); break;
    case TOK_GREATER_EQ: emitByte(parser, OP_GREATER_EQ); break;
    case TOK_LESS:       emitByte(parser, OP_LESS); break;
    case TOK_LESS_EQ:    emitByte(parser, OP_LESS_EQ); break;
    case TOK_EQ:         emitByte(parser, OP_EQ); break;
    default:             return;
  }
}

static void literal(Parser *parser, bool __attribute__((unused)) canAssign) {
  switch (parser->previous.type) {
    case TOK_FALSE: emitByte(parser, OP_FALSE); break;
    case TOK_TRUE:  emitByte(parser, OP_TRUE); break;
    case TOK_NIL:   emitByte(parser, OP_NIL); break;
    default:        return;
  }
}

static void string(Parser *parser, bool __attribute__((unused)) canAssign) {
  // +1 and -2 trims leading/trailing quotation marks. Copying literal only.
  ObjString *s = copyString(parser->vm, parser->previous.start + 1,
                            parser->previous.length - 2);

  emitConstant(parser, OBJ_VAL(s));
}

static void namedVariable(Parser *parser, Token *name, bool canAssign) {
  uint8_t getOp, setOp;
  int opArg = resolveLocal(parser, name);

  if (opArg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    opArg = identifierConstant(parser, name);
    if (opArg == -1)
      return;

    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(parser, TOK_EQ)) {
    expression(parser);
    emitBytes(parser, setOp, opArg);
    return;
  }

  emitBytes(parser, getOp, opArg);
}

static void variable(Parser *parser, bool canAssign) {
  namedVariable(parser, &parser->previous, canAssign);
}

static void declareVariable(Parser *parser) {
  if (inGlobalScope(parser))
    return;

  Token name         = parser->previous;
  Compiler *compiler = parser->currentCompiler;

  // Work backwards from the array which goes from inner most scope outwards
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];

    if (local->depth != -1 && local->depth < compiler->scopeDepth)
      break;

    if (identifiersEqual(&name, &local->name)) {
      errorAtPrevious(parser,
                      "already a variable with this name in this scope.");
    }
  }

  addLocal(parser, name);
}

// The table of parse rules that drives the parser.
ParseRule rules[] = {
    [TOK_LEFT_PAREN]  = {grouping, NULL,   PREC_NONE      },
    [TOK_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE      },
    [TOK_LEFT_BRACE]  = {NULL,     NULL,   PREC_NONE      },
    [TOK_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE      },
    [TOK_COMMA]       = {NULL,     NULL,   PREC_NONE      },
    [TOK_DOT]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_MINUS]       = {unary,    binary, PREC_TERM      },
    [TOK_PLUS]        = {NULL,     binary, PREC_TERM      },
    [TOK_SEMICOLON]   = {NULL,     NULL,   PREC_NONE      },
    [TOK_SLASH]       = {NULL,     binary, PREC_FACTOR    },
    [TOK_STAR]        = {NULL,     binary, PREC_FACTOR    },
    [TOK_BANG]        = {unary,    NULL,   PREC_UNARY     },
    [TOK_BANG_EQ]     = {NULL,     binary, PREC_COMPARISON},
    [TOK_EQ]          = {NULL,     NULL,   PREC_NONE      },
    [TOK_EQ_EQ]       = {NULL,     binary, PREC_COMPARISON},
    [TOK_GREATER]     = {NULL,     binary, PREC_COMPARISON},
    [TOK_GREATER_EQ]  = {NULL,     binary, PREC_COMPARISON},
    [TOK_LESS]        = {NULL,     binary, PREC_COMPARISON},
    [TOK_LESS_EQ]     = {NULL,     binary, PREC_COMPARISON},
    [TOK_IDENTIFIER]  = {variable, NULL,   PREC_NONE      },
    [TOK_STRING]      = {string,   NULL,   PREC_NONE      },
    [TOK_NUMBER]      = {number,   NULL,   PREC_NONE      },
    [TOK_AND]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_CLASS]       = {NULL,     NULL,   PREC_NONE      },
    [TOK_ELSE]        = {NULL,     NULL,   PREC_NONE      },
    [TOK_FALSE]       = {literal,  NULL,   PREC_NONE      },
    [TOK_FOR]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_FUN]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_IF]          = {NULL,     NULL,   PREC_NONE      },
    [TOK_NIL]         = {literal,  NULL,   PREC_NONE      },
    [TOK_OR]          = {NULL,     NULL,   PREC_NONE      },
    [TOK_PRINT]       = {NULL,     NULL,   PREC_NONE      },
    [TOK_RETURN]      = {NULL,     NULL,   PREC_NONE      },
    [TOK_SUPER]       = {NULL,     NULL,   PREC_NONE      },
    [TOK_THIS]        = {NULL,     NULL,   PREC_NONE      },
    [TOK_TRUE]        = {literal,  NULL,   PREC_NONE      },
    [TOK_VAR]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_WHILE]       = {NULL,     NULL,   PREC_NONE      },
    [TOK_ERR]         = {NULL,     NULL,   PREC_NONE      },
    [TOK_EOF]         = {NULL,     NULL,   PREC_NONE      },
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
    errorAtPrevious(parser, "expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(parser, canAssign);

  // Compile infix expression
  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(parser, canAssign);
  }

  if (canAssign && match(parser, TOK_EQ)) {
    errorAtPrevious(parser, "Invalid assignment target.");
  }
}

static int parseVariable(Parser *parser, const char *errorMessage) {
  consume(parser, TOK_IDENTIFIER, errorMessage);

  declareVariable(parser);

  // At runtime, locals are not looked up by name like globals.
  if (inLocalScope(parser))
    return -2;

  return identifierConstant(parser, &parser->previous);
}

static void markInitialized(Compiler *compiler) {
  compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Parser *parser, uint8_t lexemeIndex) {
  if (inLocalScope(parser)) {
    markInitialized(parser->currentCompiler);
    return;
  }

  emitBytes(parser, OP_DEFINE_GLOBAL, lexemeIndex);
}

static void expression(Parser *parser) {
  parsePrecedence(parser, PREC_ASSIGNMENT);
}

static void synchronize(Parser *parser) {
  parser->panicMode = false;

  // Skip tokens until we reach a statement boundary
  while (parser->current.type != TOK_EOF) {
    if (parser->previous.type == TOK_SEMICOLON)
      return;

    switch (parser->current.type) {
      case TOK_CLASS:
      case TOK_FUN:
      case TOK_VAR:
      case TOK_FOR:
      case TOK_IF:
      case TOK_WHILE:
      case TOK_PRINT:
      case TOK_RETURN: return;
      default:         advance(parser);
    }
  }
}

static void blockStatement(Parser *parser) {
  while (!check(parser, TOK_RIGHT_BRACE) && !check(parser, TOK_EOF)) {
    declarationStatement(parser);
  }

  consume(parser, TOK_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(Parser *parser) {
  uint8_t globalLexemeIndex = parseVariable(parser, "Expect variable name.");

  if (match(parser, TOK_EQ)) {
    expression(parser);
  } else {
    // variables will be initialized with 'nil' by default if not given
    emitByte(parser, OP_NIL);
  }

  consume(parser, TOK_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(parser, globalLexemeIndex);
}

static void expressionStatement(Parser *parser) {
  expression(parser);
  consume(parser, TOK_SEMICOLON, "Expect ';' after expresssion.");
  emitByte(parser, OP_POP);
}

static void printStatement(Parser *parser) {
  expression(parser);
  consume(parser, TOK_SEMICOLON, "Expect ';' after value.");
  emitByte(parser, OP_PRINT);
}

static void declarationStatement(Parser *parser) {
  if (match(parser, TOK_VAR)) {
    varDeclaration(parser);
  } else {
    statement(parser);
  }

  if (parser->panicMode) {
    synchronize(parser);
  }
}

static void statement(Parser *parser) {
  if (match(parser, TOK_PRINT)) {
    printStatement(parser);
  } else if (match(parser, TOK_LEFT_BRACE)) {
    beginScope(parser);
    blockStatement(parser);
    endScope(parser);
  } else {
    expressionStatement(parser);
  }
}

bool compile(VM *vm, const char *source) {
  Scanner scanner;
  initScanner(&scanner, source);

  Compiler compiler;
  initCompiler(&compiler);

  Parser parser;
  parser.hadError        = false;
  parser.panicMode       = false;
  parser.scanner         = &scanner;
  parser.vm              = vm;
  parser.currentCompiler = &compiler;

  advance(&parser);

  while (!match(&parser, TOK_EOF)) {
    declarationStatement(&parser);
  }

  endCompiler(&parser);

  return !parser.hadError;
}
