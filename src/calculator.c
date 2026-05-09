#include "calculator/calculator.h"

#include <math.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

CalculatorResult calculator_evaluate(const char* expression) {
    CalculatorResult result;
    result.status = CALCULATOR_OK;
    result.value = NAN;
    result.error[0] = '\0';

    if (expression == NULL) {
        result.status = CALCULATOR_ERROR_SYNTAX;
        strncpy(result.error, "Expression is NULL", CALCULATOR_ERROR_SIZE - 1);
        result.error[CALCULATOR_ERROR_SIZE - 1] = '\0';
        return result;
    }

    Parser parser;
    parser.result = &result;
    parser.depth = 0;
    lexer_init(&parser.lexer, expression, &result);

    if (result.status == CALCULATOR_OK) {
        result.value = parse_expression(&parser);
    }
    if (result.status == CALCULATOR_OK && parser.lexer.current.type != TOKEN_END) {
        result.status = CALCULATOR_ERROR_SYNTAX;
        strncpy(result.error, "Unexpected trailing input", CALCULATOR_ERROR_SIZE - 1);
        result.error[CALCULATOR_ERROR_SIZE - 1] = '\0';
    }
    if (result.status != CALCULATOR_OK) {
        result.value = NAN;
    }

    return result;
}

double parser(const char expression[]) {
    const CalculatorResult result = calculator_evaluate(expression);
    return result.status == CALCULATOR_OK ? result.value : NAN;
}
