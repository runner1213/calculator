#include "calculator/calculator_mpfr.h"

#include <gmp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer_mpfr.h"
#include "parser_mpfr.h"

typedef struct {
    CalculatorSymbolKind kind;
    char name[CALCULATOR_NAME_SIZE];
    const char* expression;
} MpfrAssignment;

static void init_result_fields(CalculatorMpfrResult* result) {
    result->status = CALCULATOR_OK;
    result->kind = CALCULATOR_RESULT_VALUE;
    result->is_complex = 0;
    result->name[0] = '\0';
    result->error[0] = '\0';
    mpfr_set_nan(result->value);
    mpfr_set_zero(result->imaginary_value, 0);
}

static void set_error(CalculatorMpfrResult* result, CalculatorStatus status, const char* message) {
    if (result->status != CALCULATOR_OK) {
        return;
    }

    result->status = status;
    strncpy(result->error, message, CALCULATOR_ERROR_SIZE - 1);
    result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
    mpfr_set_nan(result->value);
    mpfr_set_nan(result->imaginary_value);
    result->is_complex = 1;
}

static void set_result_name(CalculatorMpfrResult* result, const char* name) {
    strncpy(result->name, name, CALCULATOR_NAME_SIZE - 1);
    result->name[CALCULATOR_NAME_SIZE - 1] = '\0';
}

static char* duplicate_string(const char* value) {
    const size_t length = strlen(value);
    char* copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1);
    return copy;
}

static int add_size_checked(size_t a, size_t b, size_t* result) {
    if (a > (size_t)-1 - b) {
        return 0;
    }

    *result = a + b;
    return 1;
}

static CalculatorStatus allocate_text(size_t length, char** output) {
    *output = malloc(length + 1);
    if (*output == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    (*output)[length] = '\0';
    return CALCULATOR_OK;
}

static void free_gmp_string(char* value) {
    if (value == NULL) {
        return;
    }

    void (*free_function)(void*, size_t) = NULL;
    mp_get_memory_functions(NULL, NULL, &free_function);
    if (free_function != NULL) {
        free_function(value, strlen(value) + 1);
    } else {
        free(value);
    }
}

mpfr_prec_t calculator_mpfr_precision_for_decimal_digits(size_t digits_after_point) {
    const long double log2_10 = 3.32192809488736234787L;
    const long double guard_digits = 32.0L;
    long double bits = ((long double)digits_after_point + guard_digits) * log2_10;

    if (bits < (long double)MPFR_PREC_MIN) {
        return MPFR_PREC_MIN;
    }
    if (bits > (long double)MPFR_PREC_MAX) {
        return MPFR_PREC_MAX;
    }

    return (mpfr_prec_t)bits + 1;
}

void calculator_mpfr_context_init(CalculatorMpfrContext* context,
                                  mpfr_prec_t precision,
                                  mpfr_rnd_t rounding) {
    if (context == NULL) {
        return;
    }

    context->symbols = NULL;
    context->count = 0;
    context->capacity = 0;
    context->precision = precision;
    context->rounding = rounding;
}

void calculator_mpfr_context_set_precision(CalculatorMpfrContext* context,
                                           mpfr_prec_t precision) {
    if (context == NULL) {
        return;
    }

    context->precision = precision;
}

static CalculatorStatus refresh_constants(CalculatorMpfrContext* context,
                                          CalculatorMpfrResult* result);

CalculatorStatus calculator_mpfr_context_set_precision_checked(CalculatorMpfrContext* context,
                                                               mpfr_prec_t precision,
                                                               CalculatorMpfrResult* result) {
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    init_result_fields(result);
    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }

    context->precision = precision;
    return refresh_constants(context, result);
}

void calculator_mpfr_context_clear_symbols(CalculatorMpfrContext* context) {
    if (context == NULL) {
        return;
    }

    for (size_t i = 0; i < context->count; i++) {
        free(context->symbols[i].name);
        free(context->symbols[i].source_expression);
        mpfr_clear(context->symbols[i].value);
        mpfr_clear(context->symbols[i].imaginary_value);
    }

    context->count = 0;
}

void calculator_mpfr_context_free(CalculatorMpfrContext* context) {
    if (context == NULL) {
        return;
    }

    calculator_mpfr_context_clear_symbols(context);

    free(context->symbols);
    context->symbols = NULL;
    context->capacity = 0;
}

void calculator_mpfr_result_init(CalculatorMpfrResult* result,
                                 mpfr_prec_t precision) {
    if (result == NULL) {
        return;
    }

    mpfr_init2(result->value, precision);
    mpfr_init2(result->imaginary_value, precision);
    init_result_fields(result);
}

void calculator_mpfr_result_clear(CalculatorMpfrResult* result) {
    if (result == NULL) {
        return;
    }

    mpfr_clear(result->value);
    mpfr_clear(result->imaginary_value);
}

static int find_symbol_index(const CalculatorMpfrContext* context, const char* name, size_t* index) {
    if (context == NULL) {
        return 0;
    }

    for (size_t i = 0; i < context->count; i++) {
        if (strcmp(context->symbols[i].name, name) == 0) {
            *index = i;
            return 1;
        }
    }

    return 0;
}

static int is_builtin_or_keyword(const char* name) {
    static const char* reserved[] = {
        "abs", "ans", "const", "cos", "deg", "degrees", "exp", "i", "ln", "log",
        "null", "sin", "sqrt", "tan", "var",
        "G", "c", "h", "k", "pi"
    };

    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcmp(name, reserved[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static int ensure_capacity(CalculatorMpfrContext* context, CalculatorMpfrResult* result) {
    if (context->count < context->capacity) {
        return 1;
    }

    const size_t max_capacity = (size_t)-1 / sizeof(*context->symbols);
    if (context->capacity > max_capacity / 2) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Too many session symbols");
        return 0;
    }

    const size_t new_capacity = context->capacity == 0 ? 8 : context->capacity * 2;
    if (new_capacity > max_capacity) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Too many session symbols");
        return 0;
    }

    CalculatorMpfrSymbol* symbols = realloc(context->symbols, new_capacity * sizeof(*symbols));
    if (symbols == NULL) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Cannot allocate session symbol table");
        return 0;
    }

    context->symbols = symbols;
    context->capacity = new_capacity;
    return 1;
}

static void set_kind_result(CalculatorMpfrResult* result, CalculatorSymbolKind kind, int deleted) {
    if (kind == CALCULATOR_SYMBOL_CONST) {
        result->kind = deleted ? CALCULATOR_RESULT_CONST_DELETED : CALCULATOR_RESULT_CONST_SET;
    } else {
        result->kind = deleted ? CALCULATOR_RESULT_VAR_DELETED : CALCULATOR_RESULT_VAR_SET;
    }
}

static CalculatorStatus set_symbol(CalculatorMpfrContext* context,
                                   const char* name,
                                   const mpfr_t value,
                                   const mpfr_t imaginary_value,
                                   int is_complex,
                                   CalculatorSymbolKind kind,
                                   const char* source_expression,
                                   int allow_reserved,
                                   CalculatorMpfrResult* result) {
    if (!allow_reserved && is_builtin_or_keyword(name)) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Cannot use reserved name");
        return result->status;
    }

    char* expression_copy = NULL;
    if (kind == CALCULATOR_SYMBOL_CONST && source_expression != NULL) {
        expression_copy = duplicate_string(source_expression);
        if (expression_copy == NULL) {
            set_error(result, CALCULATOR_ERROR_MEMORY, "Cannot allocate constant expression");
            return result->status;
        }
    }

    size_t index = 0;
    if (find_symbol_index(context, name, &index)) {
        CalculatorMpfrSymbol* symbol = &context->symbols[index];
        if (symbol->kind != kind) {
            free(expression_copy);
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Name already exists with another symbol kind");
            return result->status;
        }

        mpfr_prec_round(symbol->value, context->precision, context->rounding);
        mpfr_prec_round(symbol->imaginary_value, context->precision, context->rounding);
        mpfr_set(symbol->value, value, context->rounding);
        mpfr_set(symbol->imaginary_value, imaginary_value, context->rounding);
        symbol->is_complex = is_complex && !mpfr_zero_p(imaginary_value);
        free(symbol->source_expression);
        symbol->source_expression = expression_copy;
        mpfr_set(result->value, value, context->rounding);
        mpfr_set(result->imaginary_value, imaginary_value, context->rounding);
        result->is_complex = symbol->is_complex;
        set_result_name(result, name);
        set_kind_result(result, kind, 0);
        return result->status;
    }

    if (!ensure_capacity(context, result)) {
        return result->status;
    }

    char* name_copy = duplicate_string(name);
    if (name_copy == NULL) {
        free(expression_copy);
        set_error(result, CALCULATOR_ERROR_MEMORY, "Cannot allocate symbol name");
        return result->status;
    }

    CalculatorMpfrSymbol* symbol = &context->symbols[context->count];
    symbol->name = name_copy;
    symbol->source_expression = expression_copy;
    symbol->kind = kind;
    mpfr_init2(symbol->value, context->precision);
    mpfr_init2(symbol->imaginary_value, context->precision);
    mpfr_set(symbol->value, value, context->rounding);
    mpfr_set(symbol->imaginary_value, imaginary_value, context->rounding);
    symbol->is_complex = is_complex && !mpfr_zero_p(imaginary_value);
    context->count++;

    mpfr_set(result->value, value, context->rounding);
    mpfr_set(result->imaginary_value, imaginary_value, context->rounding);
    result->is_complex = symbol->is_complex;
    set_result_name(result, name);
    set_kind_result(result, kind, 0);
    return result->status;
}

static CalculatorStatus delete_symbol(CalculatorMpfrContext* context,
                                      const char* name,
                                      CalculatorSymbolKind kind,
                                      CalculatorMpfrResult* result) {
    size_t index = 0;
    if (!find_symbol_index(context, name, &index)) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Unknown symbol");
        return result->status;
    }

    if (context->symbols[index].kind != kind) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Symbol has another kind");
        return result->status;
    }

    free(context->symbols[index].name);
    free(context->symbols[index].source_expression);
    mpfr_clear(context->symbols[index].value);
    mpfr_clear(context->symbols[index].imaginary_value);
    if (index + 1 < context->count) {
        memmove(&context->symbols[index],
                &context->symbols[index + 1],
                (context->count - index - 1) * sizeof(context->symbols[0]));
    }
    context->count--;

    set_result_name(result, name);
    set_kind_result(result, kind, 1);
    return result->status;
}

static CalculatorStatus evaluate_expression(CalculatorMpfrContext* context,
                                            const char* expression,
                                            int auto_update_var,
                                            CalculatorMpfrResult* result) {
    if (expression == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Expression is NULL");
        return result->status;
    }

    MpfrParser parser;
    parser.result = result;
    parser.context = context;
    parser.depth = 0;
    parser.auto_update_var = auto_update_var;
    parser.has_mutable_reference = 0;
    parser.has_multiple_mutable_references = 0;
    parser.mutable_reference_index = 0;
    mpfr_lexer_init(&parser.lexer, expression, result);

    MpfrComplex value;
    mpfr_init2(value.real, mpfr_get_prec(result->value));
    mpfr_init2(value.imag, mpfr_get_prec(result->value));
    mpfr_set_zero(value.real, 0);
    mpfr_set_zero(value.imag, 0);
    value.is_complex = 0;
    if (result->status == CALCULATOR_OK) {
        mpfr_parse_expression(&parser, &value);
    }
    if (result->status == CALCULATOR_OK && parser.lexer.current.type != MPFR_TOKEN_END) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Unexpected trailing input");
    }
    if (result->status == CALCULATOR_OK &&
        auto_update_var &&
        context != NULL &&
        parser.has_mutable_reference &&
        !parser.has_multiple_mutable_references) {
        CalculatorMpfrSymbol* symbol = &context->symbols[parser.mutable_reference_index];
        mpfr_prec_round(symbol->value, context->precision, context->rounding);
        mpfr_prec_round(symbol->imaginary_value, context->precision, context->rounding);
        mpfr_set(symbol->value, value.real, context->rounding);
        mpfr_set(symbol->imaginary_value, value.imag, context->rounding);
        symbol->is_complex = value.is_complex;
        result->kind = CALCULATOR_RESULT_VAR_UPDATED;
        set_result_name(result, symbol->name);
    }
    if (result->status == CALCULATOR_OK) {
        mpfr_set(result->value, value.real, context->rounding);
        mpfr_set(result->imaginary_value, value.imag, context->rounding);
        result->is_complex = value.is_complex;
    }
    if (result->status != CALCULATOR_OK) {
        mpfr_set_nan(result->value);
        mpfr_set_nan(result->imaginary_value);
        result->is_complex = 1;
    }

    mpfr_clear(value.imag);
    mpfr_clear(value.real);
    return result->status;
}

static CalculatorStatus refresh_constants(CalculatorMpfrContext* context,
                                          CalculatorMpfrResult* result) {
    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }

    for (size_t i = 0; i < context->count; i++) {
        CalculatorMpfrSymbol* symbol = &context->symbols[i];
        if (symbol->kind != CALCULATOR_SYMBOL_CONST || symbol->source_expression == NULL) {
            mpfr_prec_round(symbol->value, context->precision, context->rounding);
            mpfr_prec_round(symbol->imaginary_value, context->precision, context->rounding);
            continue;
        }

        CalculatorMpfrResult value_result;
        calculator_mpfr_result_init(&value_result, context->precision);
        evaluate_expression(context, symbol->source_expression, 0, &value_result);
        if (value_result.status != CALCULATOR_OK) {
            result->status = value_result.status;
            snprintf(result->error, CALCULATOR_ERROR_SIZE,
                     "Cannot refresh const '%s': %s",
                     symbol->name,
                     value_result.error);
            mpfr_set_nan(result->value);
            mpfr_set_nan(result->imaginary_value);
            result->is_complex = 1;
            calculator_mpfr_result_clear(&value_result);
            return result->status;
        }

        mpfr_prec_round(symbol->value, context->precision, context->rounding);
        mpfr_prec_round(symbol->imaginary_value, context->precision, context->rounding);
        mpfr_set(symbol->value, value_result.value, context->rounding);
        mpfr_set(symbol->imaginary_value, value_result.imaginary_value, context->rounding);
        symbol->is_complex = value_result.is_complex;
        calculator_mpfr_result_clear(&value_result);
    }

    return result->status;
}

CalculatorStatus calculator_mpfr_context_set_ans(CalculatorMpfrContext* context,
                                                 const mpfr_t value,
                                                 CalculatorMpfrResult* result) {
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    init_result_fields(result);
    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }

    mpfr_t zero;
    mpfr_init2(zero, context->precision);
    mpfr_set_zero(zero, 0);
    const CalculatorStatus status = set_symbol(context,
                                               "ans",
                                               value,
                                               zero,
                                               0,
                                               CALCULATOR_SYMBOL_CONST,
                                               NULL,
                                               1,
                                               result);
    mpfr_clear(zero);
    return status;
}

CalculatorStatus calculator_mpfr_context_set_complex_ans(CalculatorMpfrContext* context,
                                                         const mpfr_t value,
                                                         const mpfr_t imaginary_value,
                                                         int is_complex,
                                                         CalculatorMpfrResult* result) {
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    init_result_fields(result);
    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }

    return set_symbol(context,
                      "ans",
                      value,
                      imaginary_value,
                      is_complex,
                      CALCULATOR_SYMBOL_CONST,
                      NULL,
                      1,
                      result);
}

static int parse_assignment_header(const char* input,
                                   MpfrAssignment* assignment,
                                   CalculatorMpfrResult* result) {
    assignment->kind = CALCULATOR_SYMBOL_CONST;
    assignment->name[0] = '\0';
    assignment->expression = NULL;

    MpfrLexer lexer;
    mpfr_lexer_init(&lexer, input, result);
    if (result->status != CALCULATOR_OK || lexer.current.type != MPFR_TOKEN_IDENTIFIER) {
        return 0;
    }

    char first[CALCULATOR_NAME_SIZE];
    strncpy(first, lexer.current.text, CALCULATOR_NAME_SIZE - 1);
    first[CALCULATOR_NAME_SIZE - 1] = '\0';
    mpfr_lexer_next(&lexer);
    if (result->status != CALCULATOR_OK) {
        return 0;
    }

    if (strcmp(first, "const") == 0 || strcmp(first, "var") == 0) {
        assignment->kind = strcmp(first, "var") == 0 ? CALCULATOR_SYMBOL_VAR : CALCULATOR_SYMBOL_CONST;

        if (lexer.current.type != MPFR_TOKEN_IDENTIFIER) {
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Expected symbol name after modifier");
            return 1;
        }

        strncpy(assignment->name, lexer.current.text, CALCULATOR_NAME_SIZE - 1);
        assignment->name[CALCULATOR_NAME_SIZE - 1] = '\0';
        mpfr_lexer_next(&lexer);
        if (result->status != CALCULATOR_OK) {
            return 1;
        }

        if (lexer.current.type != MPFR_TOKEN_EQUAL) {
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Expected '=' after symbol name");
            return 1;
        }

        mpfr_lexer_next(&lexer);
        assignment->expression = input + lexer.current.position;
        return 1;
    }

    if (lexer.current.type != MPFR_TOKEN_EQUAL) {
        return 0;
    }

    assignment->kind = CALCULATOR_SYMBOL_CONST;
    strncpy(assignment->name, first, CALCULATOR_NAME_SIZE - 1);
    assignment->name[CALCULATOR_NAME_SIZE - 1] = '\0';

    mpfr_lexer_next(&lexer);
    assignment->expression = input + lexer.current.position;
    return 1;
}

static int is_null_literal(const char* expression) {
    CalculatorMpfrResult result;
    calculator_mpfr_result_init(&result, MPFR_PREC_MIN);

    MpfrLexer lexer;
    mpfr_lexer_init(&lexer, expression, &result);
    if (result.status != CALCULATOR_OK || lexer.current.type != MPFR_TOKEN_IDENTIFIER ||
        strcmp(lexer.current.text, "null") != 0) {
        calculator_mpfr_result_clear(&result);
        return 0;
    }

    mpfr_lexer_next(&lexer);
    const int is_null = result.status == CALCULATOR_OK && lexer.current.type == MPFR_TOKEN_END;
    calculator_mpfr_result_clear(&result);
    return is_null;
}

CalculatorStatus calculator_mpfr_evaluate(const char* expression,
                                          mpfr_prec_t precision,
                                          mpfr_rnd_t rounding,
                                          CalculatorMpfrResult* result) {
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    mpfr_prec_round(result->value, precision, rounding);
    mpfr_prec_round(result->imaginary_value, precision, rounding);
    init_result_fields(result);

    CalculatorMpfrContext context;
    calculator_mpfr_context_init(&context, precision, rounding);
    const CalculatorStatus status = evaluate_expression(&context, expression, 0, result);
    calculator_mpfr_context_free(&context);
    return status;
}

CalculatorStatus calculator_mpfr_context_evaluate(CalculatorMpfrContext* context,
                                                  const char* expression,
                                                  CalculatorMpfrResult* result) {
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    init_result_fields(result);
    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }
    if (expression == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Expression is NULL");
        return result->status;
    }

    mpfr_prec_round(result->value, context->precision, context->rounding);
    mpfr_prec_round(result->imaginary_value, context->precision, context->rounding);

    MpfrAssignment assignment;
    if (!parse_assignment_header(expression, &assignment, result)) {
        if (result->status != CALCULATOR_OK) {
            return result->status;
        }
        return evaluate_expression(context, expression, 1, result);
    }
    if (result->status != CALCULATOR_OK) {
        return result->status;
    }

    if (is_null_literal(assignment.expression)) {
        return delete_symbol(context, assignment.name, assignment.kind, result);
    }

    CalculatorMpfrResult value_result;
    calculator_mpfr_result_init(&value_result, context->precision);
    evaluate_expression(context, assignment.expression, 0, &value_result);
    if (value_result.status != CALCULATOR_OK) {
        result->status = value_result.status;
        strncpy(result->error, value_result.error, CALCULATOR_ERROR_SIZE - 1);
        result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
        calculator_mpfr_result_clear(&value_result);
        mpfr_set_nan(result->value);
        mpfr_set_nan(result->imaginary_value);
        result->is_complex = 1;
        return result->status;
    }

    const CalculatorStatus status = set_symbol(context,
                                               assignment.name,
                                               value_result.value,
                                               value_result.imaginary_value,
                                               value_result.is_complex,
                                               assignment.kind,
                                               assignment.expression,
                                               0,
                                               result);
    calculator_mpfr_result_clear(&value_result);
    return status;
}

CalculatorStatus calculator_mpfr_evaluate_fixed(const char* expression,
                                                size_t digits_after_point,
                                                mpfr_rnd_t rounding,
                                                CalculatorMpfrResult* result,
                                                char** output) {
    if (output == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    *output = NULL;

    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    init_result_fields(result);
    const mpfr_prec_t precision = calculator_mpfr_precision_for_decimal_digits(digits_after_point);
    CalculatorStatus status = calculator_mpfr_evaluate(expression, precision, rounding, result);
    if (status != CALCULATOR_OK) {
        return status;
    }

    return calculator_mpfr_result_format_fixed(result, digits_after_point, output);
}

CalculatorStatus calculator_mpfr_context_evaluate_fixed(CalculatorMpfrContext* context,
                                                        const char* expression,
                                                        size_t digits_after_point,
                                                        CalculatorMpfrResult* result,
                                                        char** output) {
    if (output == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    *output = NULL;

    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    init_result_fields(result);

    if (context == NULL) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result->status;
    }

    CalculatorStatus status = calculator_mpfr_context_set_precision_checked(
        context,
        calculator_mpfr_precision_for_decimal_digits(digits_after_point),
        result);
    if (status != CALCULATOR_OK) {
        return status;
    }

    status = calculator_mpfr_context_evaluate(context, expression, result);
    if (status != CALCULATOR_OK ||
        result->kind == CALCULATOR_RESULT_CONST_DELETED ||
        result->kind == CALCULATOR_RESULT_VAR_DELETED) {
        return status;
    }

    return calculator_mpfr_result_format_fixed(result, digits_after_point, output);
}

static CalculatorStatus format_mpfr_fixed_value(const mpfr_t value,
                                                size_t digits_after_point,
                                                char** output) {
    if (output == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    *output = NULL;
    if (mpfr_nan_p(value)) {
        *output = duplicate_string("nan");
        return *output == NULL ? CALCULATOR_ERROR_MEMORY : CALCULATOR_OK;
    }
    if (mpfr_inf_p(value)) {
        *output = duplicate_string(mpfr_sgn(value) < 0 ? "-inf" : "inf");
        return *output == NULL ? CALCULATOR_ERROR_MEMORY : CALCULATOR_OK;
    }

    if (digits_after_point > (size_t)INT_MAX) {
        return CALCULATOR_ERROR_MEMORY;
    }

    mpz_t scale;
    mpz_t scaled_integer;
    mpz_init(scale);
    mpz_init(scaled_integer);
    mpz_ui_pow_ui(scale, 10, (unsigned long)digits_after_point);

    const size_t scale_bits = mpz_sizeinbase(scale, 2);
    mpfr_prec_t scaled_precision = mpfr_get_prec(value);
    if ((long double)scaled_precision + (long double)scale_bits + 8.0L >
        (long double)MPFR_PREC_MAX) {
        scaled_precision = MPFR_PREC_MAX;
    } else {
        scaled_precision = (mpfr_prec_t)(scaled_precision + (mpfr_prec_t)scale_bits + 8);
    }

    mpfr_t scaled;
    mpfr_init2(scaled, scaled_precision);
    mpfr_mul_z(scaled, value, scale, MPFR_RNDN);
    mpfr_get_z(scaled_integer, scaled, MPFR_RNDN);
    mpfr_clear(scaled);
    mpz_clear(scale);

    const int negative = mpz_sgn(scaled_integer) < 0;
    if (negative) {
        mpz_neg(scaled_integer, scaled_integer);
    }

    char* digits = mpz_get_str(NULL, 10, scaled_integer);
    mpz_clear(scaled_integer);
    if (digits == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    const size_t digits_length = strlen(digits);
    size_t output_length = negative ? 1 : 0;

    if (digits_after_point == 0) {
        if (!add_size_checked(output_length, digits_length, &output_length)) {
            free_gmp_string(digits);
            return CALCULATOR_ERROR_MEMORY;
        }

        CalculatorStatus status = allocate_text(output_length, output);
        if (status != CALCULATOR_OK) {
            free_gmp_string(digits);
            return status;
        }

        char* cursor = *output;
        if (negative) {
            *cursor++ = '-';
        }
        memcpy(cursor, digits, digits_length);
        free_gmp_string(digits);
        return CALCULATOR_OK;
    }

    const int has_integer_digits = digits_length > digits_after_point;
    const size_t integer_digits = has_integer_digits ? digits_length - digits_after_point : 1;
    const size_t leading_fraction_zeroes = has_integer_digits ? 0 : digits_after_point - digits_length;
    const size_t copied_fraction_digits = has_integer_digits ? digits_after_point : digits_length;

    if (!add_size_checked(output_length, integer_digits, &output_length) ||
        !add_size_checked(output_length, 1, &output_length) ||
        !add_size_checked(output_length, leading_fraction_zeroes, &output_length) ||
        !add_size_checked(output_length, copied_fraction_digits, &output_length)) {
        free_gmp_string(digits);
        return CALCULATOR_ERROR_MEMORY;
    }

    CalculatorStatus status = allocate_text(output_length, output);
    if (status != CALCULATOR_OK) {
        free_gmp_string(digits);
        return status;
    }

    char* cursor = *output;
    if (negative) {
        *cursor++ = '-';
    }

    if (has_integer_digits) {
        memcpy(cursor, digits, integer_digits);
        cursor += integer_digits;
    } else {
        *cursor++ = '0';
    }

    *cursor++ = '.';
    memset(cursor, '0', leading_fraction_zeroes);
    cursor += leading_fraction_zeroes;

    if (copied_fraction_digits > 0) {
        const char* fraction_start = has_integer_digits ? digits + integer_digits : digits;
        memcpy(cursor, fraction_start, copied_fraction_digits);
    }

    free_gmp_string(digits);
    return CALCULATOR_OK;
}

CalculatorStatus calculator_mpfr_result_format_fixed(const CalculatorMpfrResult* result,
                                                     size_t digits_after_point,
                                                     char** output) {
    if (output == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    *output = NULL;
    if (result == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }
    if (result->status != CALCULATOR_OK) {
        return result->status;
    }
    if (!result->is_complex) {
        return format_mpfr_fixed_value(result->value, digits_after_point, output);
    }

    char* real = NULL;
    char* imag = NULL;
    mpfr_t imag_abs;
    mpfr_init2(imag_abs, mpfr_get_prec(result->imaginary_value));
    mpfr_abs(imag_abs, result->imaginary_value, MPFR_RNDN);

    CalculatorStatus status = format_mpfr_fixed_value(result->value, digits_after_point, &real);
    if (status == CALCULATOR_OK) {
        status = format_mpfr_fixed_value(imag_abs, digits_after_point, &imag);
    }
    mpfr_clear(imag_abs);
    if (status != CALCULATOR_OK) {
        free(real);
        free(imag);
        return status;
    }

    const char* sign = mpfr_sgn(result->imaginary_value) < 0 ? " - " : " + ";
    size_t output_length = 0;
    if (!add_size_checked(strlen(real), strlen(sign), &output_length) ||
        !add_size_checked(output_length, strlen(imag), &output_length) ||
        !add_size_checked(output_length, 1, &output_length)) {
        free(real);
        free(imag);
        return CALCULATOR_ERROR_MEMORY;
    }

    status = allocate_text(output_length, output);
    if (status != CALCULATOR_OK) {
        free(real);
        free(imag);
        return status;
    }

    snprintf(*output, output_length + 1, "%s%s%si", real, sign, imag);
    free(real);
    free(imag);
    return CALCULATOR_OK;
}

void calculator_mpfr_free_string(char* value) {
    free(value);
}
