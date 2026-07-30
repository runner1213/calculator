#include "calculator/calculator_mpfr.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lexer_mpfr.h"
#include "parser_mpfr.h"

typedef struct {
    int is_assignment;
    CalculatorSymbolKind kind;
    char name[CALCULATOR_NAME_SIZE];
    const char* expression;
} MpfrAssignment;

static void init_result_fields(CalculatorMpfrResult* result) {
    result->status = CALCULATOR_OK;
    result->kind = CALCULATOR_RESULT_VALUE;
    result->name[0] = '\0';
    result->error[0] = '\0';
    mpfr_set_nan(result->value);
}

static void set_error(CalculatorMpfrResult* result, CalculatorStatus status, const char* message) {
    if (result->status != CALCULATOR_OK) {
        return;
    }

    result->status = status;
    strncpy(result->error, message, CALCULATOR_ERROR_SIZE - 1);
    result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
    mpfr_set_nan(result->value);
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

void calculator_mpfr_context_free(CalculatorMpfrContext* context) {
    if (context == NULL) {
        return;
    }

    for (size_t i = 0; i < context->count; i++) {
        free(context->symbols[i].name);
        mpfr_clear(context->symbols[i].value);
    }

    free(context->symbols);
    context->symbols = NULL;
    context->count = 0;
    context->capacity = 0;
}

void calculator_mpfr_result_init(CalculatorMpfrResult* result,
                                 mpfr_prec_t precision) {
    if (result == NULL) {
        return;
    }

    mpfr_init2(result->value, precision);
    init_result_fields(result);
}

void calculator_mpfr_result_clear(CalculatorMpfrResult* result) {
    if (result == NULL) {
        return;
    }

    mpfr_clear(result->value);
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
        "abs", "const", "cos", "deg", "degrees", "exp", "ln", "log",
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

    const size_t new_capacity = context->capacity == 0 ? 8 : context->capacity * 2;
    if (new_capacity < context->capacity) {
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
                                   CalculatorSymbolKind kind,
                                   CalculatorMpfrResult* result) {
    if (is_builtin_or_keyword(name)) {
        set_error(result, CALCULATOR_ERROR_SYNTAX, "Cannot use reserved name");
        return result->status;
    }

    size_t index = 0;
    if (find_symbol_index(context, name, &index)) {
        CalculatorMpfrSymbol* symbol = &context->symbols[index];
        if (symbol->kind != kind) {
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Name already exists with another symbol kind");
            return result->status;
        }

        mpfr_prec_round(symbol->value, context->precision, context->rounding);
        mpfr_set(symbol->value, value, context->rounding);
        mpfr_set(result->value, value, context->rounding);
        set_result_name(result, name);
        set_kind_result(result, kind, 0);
        return result->status;
    }

    if (!ensure_capacity(context, result)) {
        return result->status;
    }

    char* name_copy = duplicate_string(name);
    if (name_copy == NULL) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Cannot allocate symbol name");
        return result->status;
    }

    CalculatorMpfrSymbol* symbol = &context->symbols[context->count];
    symbol->name = name_copy;
    symbol->kind = kind;
    mpfr_init2(symbol->value, context->precision);
    mpfr_set(symbol->value, value, context->rounding);
    context->count++;

    mpfr_set(result->value, value, context->rounding);
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
    mpfr_clear(context->symbols[index].value);
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

    if (result->status == CALCULATOR_OK) {
        mpfr_parse_expression(&parser, result->value);
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
        mpfr_set(symbol->value, result->value, context->rounding);
        result->kind = CALCULATOR_RESULT_VAR_UPDATED;
        set_result_name(result, symbol->name);
    }
    if (result->status != CALCULATOR_OK) {
        mpfr_set_nan(result->value);
    }

    return result->status;
}

static int parse_assignment_header(const char* input,
                                   MpfrAssignment* assignment,
                                   CalculatorMpfrResult* result) {
    assignment->is_assignment = 0;
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
        assignment->is_assignment = 1;
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

    assignment->is_assignment = 1;
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
        return result->status;
    }

    const CalculatorStatus status = set_symbol(context, assignment.name, value_result.value, assignment.kind, result);
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

    calculator_mpfr_context_set_precision(
        context,
        calculator_mpfr_precision_for_decimal_digits(digits_after_point));

    CalculatorStatus status = calculator_mpfr_context_evaluate(context, expression, result);
    if (status != CALCULATOR_OK ||
        result->kind == CALCULATOR_RESULT_CONST_DELETED ||
        result->kind == CALCULATOR_RESULT_VAR_DELETED) {
        return status;
    }

    return calculator_mpfr_result_format_fixed(result, digits_after_point, output);
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
    if (mpfr_nan_p(result->value)) {
        *output = duplicate_string("nan");
        return *output == NULL ? CALCULATOR_ERROR_MEMORY : CALCULATOR_OK;
    }
    if (mpfr_inf_p(result->value)) {
        *output = duplicate_string(mpfr_sgn(result->value) < 0 ? "-inf" : "inf");
        return *output == NULL ? CALCULATOR_ERROR_MEMORY : CALCULATOR_OK;
    }

    if (digits_after_point > (size_t)INT_MAX) {
        return CALCULATOR_ERROR_MEMORY;
    }

    char* formatted = NULL;
    if (mpfr_asprintf(&formatted, "%.*Rf", (int)digits_after_point, result->value) < 0 ||
        formatted == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    char* text = duplicate_string(formatted);
    mpfr_free_str(formatted);
    if (text == NULL) {
        return CALCULATOR_ERROR_MEMORY;
    }

    *output = text;
    return CALCULATOR_OK;
}

void calculator_mpfr_free_string(char* value) {
    free(value);
}
