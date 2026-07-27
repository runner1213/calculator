#ifndef CALCULATOR_PARSER_MPFR_H
#define CALCULATOR_PARSER_MPFR_H

#include "calculator/calculator_mpfr.h"
#include "lexer_mpfr.h"

typedef struct {
    MpfrLexer lexer;
    CalculatorMpfrResult* result;
    CalculatorMpfrContext* context;
    unsigned int depth;
    int auto_update_var;
    int has_mutable_reference;
    int has_multiple_mutable_references;
    size_t mutable_reference_index;
} MpfrParser;

void mpfr_parse_expression(MpfrParser* parser, mpfr_t value);

#endif
