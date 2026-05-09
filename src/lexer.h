#ifndef CALCULATOR_LEXER_H
#define CALCULATOR_LEXER_H

#include <stddef.h>

#include "calculator/calculator.h"

#define CALCULATOR_IDENTIFIER_SIZE 32

typedef enum {
    TOKEN_END = 0,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_CARET,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    double number;
    char text[CALCULATOR_IDENTIFIER_SIZE];
    size_t position;
} Token;

typedef struct {
    const char* input;
    size_t position;
    Token current;
    CalculatorResult* result;
} Lexer;

void lexer_init(Lexer* lexer, const char* input, CalculatorResult* result);
void lexer_next(Lexer* lexer);

#endif
