#ifndef CALCULATOR_PARSER_H
#define CALCULATOR_PARSER_H

#include "calculator/calculator.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    CalculatorResult* result;
    CalculatorContext* context;
    unsigned int depth;
    int auto_update_var;
    int has_mutable_reference;
    int has_multiple_mutable_references;
    size_t mutable_reference_index;
} Parser;

double parse_expression(Parser* parser);

#endif
