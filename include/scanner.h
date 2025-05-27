#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

typedef struct scanner {
  const char *start;   // Start character of current lexeme in source.
  const char *current; // Current character in source.
  int line;
} Scanner;

typedef enum token_type {
  // Single-character tokens.
  TOK_LEFT_PAREN,
  TOK_RIGHT_PAREN,
  TOK_LEFT_BRACE,
  TOK_RIGHT_BRACE,
  TOK_COMMA,
  TOK_DOT,
  TOK_MINUS,
  TOK_PLUS,
  TOK_SEMICOLON,
  TOK_SLASH,
  TOK_STAR,

  // One or two character tokens.
  TOK_BANG,
  TOK_BANG_EQ,
  TOK_EQ,
  TOK_EQ_EQ,
  TOK_GREATER,
  TOK_GREATER_EQ,
  TOK_LESS,
  TOK_LESS_EQ,

  // Literals.
  TOK_IDENTIFIER,
  TOK_STRING,
  TOK_NUMBER,

  // Keywords.
  TOK_AND,
  TOK_CLASS,
  TOK_ELSE,
  TOK_FALSE,
  TOK_FOR,
  TOK_FUN,
  TOK_IF,
  TOK_NIL,
  TOK_OR,
  TOK_PRINT,
  TOK_RETURN,
  TOK_SUPER,
  TOK_THIS,
  TOK_TRUE,
  TOK_VAR,
  TOK_WHILE,

  // Misc.
  TOK_ERR,
  TOK_EOF
} TokenType;

typedef struct token {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

void initScanner(Scanner *scanner, const char *source);

Token scanToken(Scanner *scanner);

#endif
