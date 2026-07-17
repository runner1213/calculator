#ifndef CALCULATOR_CALCULATOR_H
#define CALCULATOR_CALCULATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALCULATOR_ERROR_SIZE 128
#define CALCULATOR_NAME_SIZE 128

typedef enum {
    CALCULATOR_OK = 0,
    CALCULATOR_ERROR_SYNTAX,
    CALCULATOR_ERROR_DOMAIN,
    CALCULATOR_ERROR_OVERFLOW,
    CALCULATOR_ERROR_MEMORY
} CalculatorStatus;

typedef enum {
    CALCULATOR_RESULT_VALUE = 0,
    CALCULATOR_RESULT_CONST_SET,
    CALCULATOR_RESULT_CONST_DELETED,
    CALCULATOR_RESULT_VAR_SET,
    CALCULATOR_RESULT_VAR_DELETED,
    CALCULATOR_RESULT_VAR_UPDATED
} CalculatorResultKind;

typedef enum {
    CALCULATOR_SYMBOL_CONST = 0,
    CALCULATOR_SYMBOL_VAR
} CalculatorSymbolKind;

typedef struct {
    char* name;
    double value;
    CalculatorSymbolKind kind;
} CalculatorSymbol;

typedef struct {
    CalculatorSymbol* symbols;
    size_t count;
    size_t capacity;
} CalculatorContext;

typedef struct {
    CalculatorStatus status;
    CalculatorResultKind kind;
    double value;
    char name[CALCULATOR_NAME_SIZE];
    char error[CALCULATOR_ERROR_SIZE];
} CalculatorResult;

void calculator_context_init(CalculatorContext* context);
void calculator_context_free(CalculatorContext* context);

CalculatorResult calculator_evaluate(const char* expression);
CalculatorResult calculator_context_evaluate(CalculatorContext* context, const char* expression);

#ifdef __cplusplus
}
#endif

#endif
