#ifndef CALCULATOR_LEXER_MPFR_H
#define CALCULATOR_LEXER_MPFR_H

#include <stddef.h>

#include "calculator/calculator_mpfr.h"

#define CALCULATOR_MPFR_IDENTIFIER_SIZE CALCULATOR_NAME_SIZE

typedef enum {
    MPFR_TOKEN_END = 0,
    MPFR_TOKEN_NUMBER,
    MPFR_TOKEN_IDENTIFIER,
    MPFR_TOKEN_PLUS,
    MPFR_TOKEN_MINUS,
    MPFR_TOKEN_STAR,
    MPFR_TOKEN_SLASH,
    MPFR_TOKEN_CARET,
    MPFR_TOKEN_LPAREN,
    MPFR_TOKEN_RPAREN,
    MPFR_TOKEN_BANG,
    MPFR_TOKEN_COMMA,
    MPFR_TOKEN_EQUAL,
    MPFR_TOKEN_INVALID
} MpfrTokenType;

typedef struct {
    MpfrTokenType type;
    const char* number_start;
    size_t number_length;
    char text[CALCULATOR_MPFR_IDENTIFIER_SIZE];
    size_t position;
} MpfrToken;

typedef struct {
    const char* input;
    size_t position;
    MpfrToken current;
    CalculatorMpfrResult* result;
} MpfrLexer;

void mpfr_lexer_init(MpfrLexer* lexer, const char* input, CalculatorMpfrResult* result);
void mpfr_lexer_next(MpfrLexer* lexer);

#endif
