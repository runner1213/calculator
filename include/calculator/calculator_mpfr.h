#ifndef CALCULATOR_CALCULATOR_MPFR_H
#define CALCULATOR_CALCULATOR_MPFR_H

#include <stddef.h>

#include <mpfr.h>

#include "calculator/calculator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* name;
    mpfr_t value;
    CalculatorSymbolKind kind;
} CalculatorMpfrSymbol;

typedef struct {
    CalculatorMpfrSymbol* symbols;
    size_t count;
    size_t capacity;
    mpfr_prec_t precision;
    mpfr_rnd_t rounding;
} CalculatorMpfrContext;

typedef struct {
    CalculatorStatus status;
    CalculatorResultKind kind;
    mpfr_t value;
    char name[CALCULATOR_NAME_SIZE];
    char error[CALCULATOR_ERROR_SIZE];
} CalculatorMpfrResult;

mpfr_prec_t calculator_mpfr_precision_for_decimal_digits(size_t digits_after_point);

void calculator_mpfr_context_init(CalculatorMpfrContext* context,
                                  mpfr_prec_t precision,
                                  mpfr_rnd_t rounding);
void calculator_mpfr_context_set_precision(CalculatorMpfrContext* context,
                                           mpfr_prec_t precision);
void calculator_mpfr_context_free(CalculatorMpfrContext* context);

void calculator_mpfr_result_init(CalculatorMpfrResult* result,
                                 mpfr_prec_t precision);
void calculator_mpfr_result_clear(CalculatorMpfrResult* result);

CalculatorStatus calculator_mpfr_evaluate(const char* expression,
                                          mpfr_prec_t precision,
                                          mpfr_rnd_t rounding,
                                          CalculatorMpfrResult* result);
CalculatorStatus calculator_mpfr_context_evaluate(CalculatorMpfrContext* context,
                                                  const char* expression,
                                                  CalculatorMpfrResult* result);
CalculatorStatus calculator_mpfr_evaluate_fixed(const char* expression,
                                                size_t digits_after_point,
                                                mpfr_rnd_t rounding,
                                                CalculatorMpfrResult* result,
                                                char** output);
CalculatorStatus calculator_mpfr_context_evaluate_fixed(CalculatorMpfrContext* context,
                                                        const char* expression,
                                                        size_t digits_after_point,
                                                        CalculatorMpfrResult* result,
                                                        char** output);

CalculatorStatus calculator_mpfr_result_format_fixed(const CalculatorMpfrResult* result,
                                                     size_t digits_after_point,
                                                     char** output);
void calculator_mpfr_free_string(char* value);

#ifdef __cplusplus
}
#endif

#endif
