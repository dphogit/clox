#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

void initScanner(Scanner *scanner, const char *source) {
  scanner->start   = source;
  scanner->current = source;
  scanner->line    = 1;
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static char peek(Scanner *scanner) { return *scanner->current; }

static char peekNext(Scanner *scanner) {
  return isAtEnd(scanner) ? '\0' : scanner->current[1];
}

static char advance(Scanner *scanner) {
  scanner->current++;
  return scanner->current[-1];
}

static bool match(Scanner *scanner, char expected) {
  if (isAtEnd(scanner) || *scanner->current != expected)
    return false;

  scanner->current++;
  return true;
}

static void skipWhitespace(Scanner *scanner) {
#define CONSUME_INLINE_COMMENT()                     \
  while (peek(scanner) != '\n' && !isAtEnd(scanner)) \
  advance(scanner)

  while (true) {
    switch (peek(scanner)) {
      case '\n':
        scanner->line++;
        advance(scanner);
        continue;
      case '/':
        if (peekNext(scanner) == '/') {
          CONSUME_INLINE_COMMENT();
          return;
        }
        continue;
      case ' ':
      case '\r':
      case '\t': advance(scanner); continue;
      default:   return;
    }
  }

#undef CONSUME_INLINE_COMMENT
}

static Token newToken(Scanner *scanner, TokenType type) {
  Token token;
  token.type   = type;
  token.start  = scanner->start;
  token.length = scanner->current - scanner->start;
  token.line   = scanner->line;
  return token;
}

static Token errorToken(Scanner *scanner, const char *message) {
  Token token;
  token.type   = TOK_ERR;
  token.start  = message;
  token.length = strlen(message);
  token.line   = scanner->line;
  return token;
}

static Token string(Scanner *scanner) {
  char c;
  while ((c = peek(scanner)) != '"' && !isAtEnd(scanner)) {
    if (c == '\n') {
      scanner->line++;
    }

    advance(scanner);
  }

  if (isAtEnd(scanner)) {
    return errorToken(scanner, "Unterminated string.");
  }

  advance(scanner);
  return newToken(scanner, TOK_STRING);
}

static Token number(Scanner *scanner) {
#define CONSUME_DIGITS()         \
  while (isdigit(peek(scanner))) \
  advance(scanner)

  CONSUME_DIGITS();

  // Handle fractional part if any.
  if (peek(scanner) == '.' && isdigit(peekNext(scanner))) {
    advance(scanner); // Consume the '.'

    CONSUME_DIGITS();
  }

  return newToken(scanner, TOK_NUMBER);

#undef CONSUME_DIGITS
}

static TokenType checkKeyword(Scanner *scanner, int start, int len,
                              const char *rest, TokenType type) {
  bool sameLength   = scanner->current - scanner->start == start + len;
  bool sameContents = strncmp(scanner->start + start, rest, len) == 0;

  return sameLength && sameContents ? type : TOK_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  switch (scanner->start[0]) {
    case 'a': return checkKeyword(scanner, 1, 2, "nd", TOK_AND);
    case 'c': return checkKeyword(scanner, 1, 4, "lass", TOK_CLASS);
    case 'e': return checkKeyword(scanner, 1, 3, "lse", TOK_ELSE);
    case 'f':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'a': return checkKeyword(scanner, 2, 3, "lse", TOK_FALSE);
          case 'o': return checkKeyword(scanner, 2, 1, "r", TOK_FOR);
          case 'u': return checkKeyword(scanner, 2, 1, "n", TOK_FUN);
        }
      }
      break;
    case 'i': return checkKeyword(scanner, 1, 1, "f", TOK_IF);
    case 'n': return checkKeyword(scanner, 1, 2, "il", TOK_NIL);
    case 'o': return checkKeyword(scanner, 1, 1, "r", TOK_OR);
    case 'p': return checkKeyword(scanner, 1, 4, "rint", TOK_PRINT);
    case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOK_RETURN);
    case 's': return checkKeyword(scanner, 1, 4, "uper", TOK_SUPER);
    case 't':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'h': return checkKeyword(scanner, 2, 2, "is", TOK_THIS);
          case 'r': return checkKeyword(scanner, 2, 2, "ue", TOK_TRUE);
        }
      }
      break;
    case 'v': return checkKeyword(scanner, 1, 2, "ar", TOK_VAR);
    case 'w': return checkKeyword(scanner, 1, 4, "hile", TOK_WHILE);
  }

  return TOK_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  while (isalnum(peek(scanner)))
    advance(scanner);

  return newToken(scanner, identifierType(scanner));
}

Token scanToken(Scanner *scanner) {
  skipWhitespace(scanner);
  scanner->start = scanner->current;

  if (isAtEnd(scanner))
    return newToken(scanner, TOK_EOF);

  char c = advance(scanner);

  if (isalpha(c))
    return identifier(scanner);

  if (isdigit(c))
    return number(scanner);

  switch (c) {
    case '(': return newToken(scanner, TOK_LEFT_PAREN);
    case ')': return newToken(scanner, TOK_RIGHT_PAREN);
    case '{': return newToken(scanner, TOK_LEFT_BRACE);
    case '}': return newToken(scanner, TOK_RIGHT_BRACE);
    case ';': return newToken(scanner, TOK_SEMICOLON);
    case ',': return newToken(scanner, TOK_COMMA);
    case '.': return newToken(scanner, TOK_DOT);
    case '-': return newToken(scanner, TOK_MINUS);
    case '+': return newToken(scanner, TOK_PLUS);
    case '/': return newToken(scanner, TOK_SLASH);
    case '*': return newToken(scanner, TOK_STAR);
    case '"': return string(scanner);
    case '!':
      return newToken(scanner, match(scanner, '=') ? TOK_BANG_EQ : TOK_BANG);
    case '=':
      return newToken(scanner, match(scanner, '=') ? TOK_EQ_EQ : TOK_EQ);
    case '<':
      return newToken(scanner, match(scanner, '=') ? TOK_LESS_EQ : TOK_LESS);
    case '>':
      return newToken(scanner,
                      match(scanner, '=') ? TOK_GREATER_EQ : TOK_GREATER);
  }

  return errorToken(scanner, "Unexpected character.");
}
