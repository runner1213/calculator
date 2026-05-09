#ifndef CALCULATOR_PARSER_H
#define CALCULATOR_PARSER_H

#include "calculator/calculator.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    CalculatorResult* result;
    unsigned int depth;
} Parser;

double parse_expression(Parser* parser);

#endif
