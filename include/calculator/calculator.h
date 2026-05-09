#ifndef CALCULATOR_CALCULATOR_H
#define CALCULATOR_CALCULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#define CALCULATOR_ERROR_SIZE 128

typedef enum {
    CALCULATOR_OK = 0,
    CALCULATOR_ERROR_SYNTAX,
    CALCULATOR_ERROR_DOMAIN,
    CALCULATOR_ERROR_OVERFLOW,
    CALCULATOR_ERROR_MEMORY
} CalculatorStatus;

typedef struct {
    CalculatorStatus status;
    double value;
    char error[CALCULATOR_ERROR_SIZE];
} CalculatorResult;

CalculatorResult calculator_evaluate(const char* expression);

#ifdef __cplusplus
}
#endif

#endif
