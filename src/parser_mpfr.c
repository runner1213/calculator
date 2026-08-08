#include "parser_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARSE_DEPTH 512
#define PHYSICS_C "299792458"
#define PHYSICS_G "6.67430e-11"
#define PHYSICS_H "6.62607015e-34"
#define PHYSICS_K "1.380649e-23"

static mpfr_rnd_t parser_rounding(const MpfrParser* parser) {
    return parser->context != NULL ? parser->context->rounding : MPFR_RNDN;
}

static mpfr_prec_t complex_precision(const MpfrComplex* value) {
    return mpfr_get_prec(value->real);
}

static void complex_init(MpfrComplex* value, mpfr_prec_t precision) {
    mpfr_init2(value->real, precision);
    mpfr_init2(value->imag, precision);
    value->is_complex = 0;
    mpfr_set_zero(value->real, 0);
    mpfr_set_zero(value->imag, 0);
}

static void complex_clear(MpfrComplex* value) {
    mpfr_clear(value->real);
    mpfr_clear(value->imag);
}

static void complex_normalize(MpfrComplex* value) {
    if (mpfr_zero_p(value->imag)) {
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
    } else {
        value->is_complex = 1;
    }
}

static void complex_set_nan(MpfrComplex* value) {
    mpfr_set_nan(value->real);
    mpfr_set_nan(value->imag);
    value->is_complex = 1;
}

static void complex_set_ui(MpfrComplex* value, unsigned long real, mpfr_rnd_t rnd) {
    mpfr_set_ui(value->real, real, rnd);
    mpfr_set_zero(value->imag, 0);
    value->is_complex = 0;
}

static void complex_copy(MpfrComplex* target, const MpfrComplex* source, mpfr_rnd_t rnd) {
    mpfr_set(target->real, source->real, rnd);
    mpfr_set(target->imag, source->imag, rnd);
    target->is_complex = source->is_complex;
    complex_normalize(target);
}

static void set_error(MpfrParser* parser, CalculatorStatus status, const char* message) {
    if (parser->result->status != CALCULATOR_OK) {
        return;
    }

    parser->result->status = status;
    snprintf(parser->result->error, CALCULATOR_ERROR_SIZE, "%s at position %zu",
             message, parser->lexer.current.position);
    mpfr_set_nan(parser->result->value);
    mpfr_set_nan(parser->result->imaginary_value);
    parser->result->is_complex = 1;
}

static int enter(MpfrParser* parser) {
    if (++parser->depth > MAX_PARSE_DEPTH) {
        set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expression nesting is too deep");
        return 0;
    }
    return 1;
}

static void leave(MpfrParser* parser) {
    if (parser->depth > 0) {
        parser->depth--;
    }
}

static int accept(MpfrParser* parser, MpfrTokenType type) {
    if (parser->lexer.current.type != type) {
        return 0;
    }
    mpfr_lexer_next(&parser->lexer);
    return 1;
}

static int is_imaginary_unit_token(const MpfrToken* token) {
    return token->type == MPFR_TOKEN_IDENTIFIER && strcmp(token->text, "i") == 0;
}

static int check_number_result(MpfrParser* parser, const mpfr_t value, const char* domain_message) {
    if (mpfr_nan_p(value)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, domain_message);
        return 0;
    }
    if (mpfr_inf_p(value)) {
        set_error(parser, CALCULATOR_ERROR_OVERFLOW, "Result is outside supported MPFR exponent range");
        return 0;
    }
    return 1;
}

static int check_complex_result(MpfrParser* parser, MpfrComplex* value, const char* domain_message) {
    if (!check_number_result(parser, value->real, domain_message)) {
        complex_set_nan(value);
        return 0;
    }
    if (!check_number_result(parser, value->imag, domain_message)) {
        complex_set_nan(value);
        return 0;
    }

    complex_normalize(value);
    return 1;
}

static void complex_add(MpfrParser* parser,
                        MpfrComplex* result,
                        const MpfrComplex* right) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    mpfr_add(result->real, result->real, right->real, rnd);
    mpfr_add(result->imag, result->imag, right->imag, rnd);
    check_complex_result(parser, result, "Arithmetic result is undefined");
}

static void complex_sub(MpfrParser* parser,
                        MpfrComplex* result,
                        const MpfrComplex* right) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    mpfr_sub(result->real, result->real, right->real, rnd);
    mpfr_sub(result->imag, result->imag, right->imag, rnd);
    check_complex_result(parser, result, "Arithmetic result is undefined");
}

static void complex_neg(MpfrParser* parser, MpfrComplex* value) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    mpfr_neg(value->real, value->real, rnd);
    mpfr_neg(value->imag, value->imag, rnd);
    complex_normalize(value);
}

static void complex_mul(MpfrParser* parser,
                        MpfrComplex* result,
                        const MpfrComplex* left,
                        const MpfrComplex* right) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t real;
    mpfr_t imag;
    mpfr_t temp;

    mpfr_init2(real, precision);
    mpfr_init2(imag, precision);
    mpfr_init2(temp, precision);

    mpfr_mul(real, left->real, right->real, rnd);
    mpfr_mul(temp, left->imag, right->imag, rnd);
    mpfr_sub(real, real, temp, rnd);

    mpfr_mul(imag, left->real, right->imag, rnd);
    mpfr_mul(temp, left->imag, right->real, rnd);
    mpfr_add(imag, imag, temp, rnd);

    mpfr_set(result->real, real, rnd);
    mpfr_set(result->imag, imag, rnd);
    result->is_complex = left->is_complex || right->is_complex;
    check_complex_result(parser, result, "Arithmetic result is undefined");

    mpfr_clear(temp);
    mpfr_clear(imag);
    mpfr_clear(real);
}

static void complex_div(MpfrParser* parser,
                        MpfrComplex* result,
                        const MpfrComplex* left,
                        const MpfrComplex* right) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t denom;
    mpfr_t real;
    mpfr_t imag;
    mpfr_t temp;

    mpfr_init2(denom, precision);
    mpfr_init2(real, precision);
    mpfr_init2(imag, precision);
    mpfr_init2(temp, precision);

    mpfr_mul(denom, right->real, right->real, rnd);
    mpfr_mul(temp, right->imag, right->imag, rnd);
    mpfr_add(denom, denom, temp, rnd);
    if (mpfr_zero_p(denom)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Division by zero");
        complex_set_nan(result);
        mpfr_clear(temp);
        mpfr_clear(imag);
        mpfr_clear(real);
        mpfr_clear(denom);
        return;
    }

    mpfr_mul(real, left->real, right->real, rnd);
    mpfr_mul(temp, left->imag, right->imag, rnd);
    mpfr_add(real, real, temp, rnd);
    mpfr_div(real, real, denom, rnd);

    mpfr_mul(imag, left->imag, right->real, rnd);
    mpfr_mul(temp, left->real, right->imag, rnd);
    mpfr_sub(imag, imag, temp, rnd);
    mpfr_div(imag, imag, denom, rnd);

    mpfr_set(result->real, real, rnd);
    mpfr_set(result->imag, imag, rnd);
    result->is_complex = left->is_complex || right->is_complex;
    check_complex_result(parser, result, "Arithmetic result is undefined");

    mpfr_clear(temp);
    mpfr_clear(imag);
    mpfr_clear(real);
    mpfr_clear(denom);
}

static void complex_abs_value(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    mpfr_hypot(result->real, arg->real, arg->imag, rnd);
    mpfr_set_zero(result->imag, 0);
    result->is_complex = 0;
    check_complex_result(parser, result, "Function result is undefined");
}

static void complex_ln(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t magnitude;

    mpfr_init2(magnitude, precision);
    mpfr_hypot(magnitude, arg->real, arg->imag, rnd);
    if (mpfr_zero_p(magnitude)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "ln() argument must be non-zero");
        complex_set_nan(result);
        mpfr_clear(magnitude);
        return;
    }

    mpfr_log(result->real, magnitude, rnd);
    mpfr_atan2(result->imag, arg->imag, arg->real, rnd);
    result->is_complex = 1;
    check_complex_result(parser, result, "Function result is undefined");

    mpfr_clear(magnitude);
}

static void complex_exp(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t scale;
    mpfr_t sin_imag;
    mpfr_t cos_imag;

    mpfr_init2(scale, precision);
    mpfr_init2(sin_imag, precision);
    mpfr_init2(cos_imag, precision);

    mpfr_exp(scale, arg->real, rnd);
    mpfr_sin_cos(sin_imag, cos_imag, arg->imag, rnd);
    mpfr_mul(result->real, scale, cos_imag, rnd);
    mpfr_mul(result->imag, scale, sin_imag, rnd);
    result->is_complex = arg->is_complex;
    check_complex_result(parser, result, "Function result is undefined");

    mpfr_clear(cos_imag);
    mpfr_clear(sin_imag);
    mpfr_clear(scale);
}

static void complex_sin(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t sin_real;
    mpfr_t cos_real;
    mpfr_t sinh_imag;
    mpfr_t cosh_imag;

    mpfr_init2(sin_real, precision);
    mpfr_init2(cos_real, precision);
    mpfr_init2(sinh_imag, precision);
    mpfr_init2(cosh_imag, precision);

    mpfr_sin_cos(sin_real, cos_real, arg->real, rnd);
    mpfr_sinh_cosh(sinh_imag, cosh_imag, arg->imag, rnd);
    mpfr_mul(result->real, sin_real, cosh_imag, rnd);
    mpfr_mul(result->imag, cos_real, sinh_imag, rnd);
    result->is_complex = arg->is_complex;
    check_complex_result(parser, result, "Function result is undefined");

    mpfr_clear(cosh_imag);
    mpfr_clear(sinh_imag);
    mpfr_clear(cos_real);
    mpfr_clear(sin_real);
}

static void complex_cos(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t sin_real;
    mpfr_t cos_real;
    mpfr_t sinh_imag;
    mpfr_t cosh_imag;

    mpfr_init2(sin_real, precision);
    mpfr_init2(cos_real, precision);
    mpfr_init2(sinh_imag, precision);
    mpfr_init2(cosh_imag, precision);

    mpfr_sin_cos(sin_real, cos_real, arg->real, rnd);
    mpfr_sinh_cosh(sinh_imag, cosh_imag, arg->imag, rnd);
    mpfr_mul(result->real, cos_real, cosh_imag, rnd);
    mpfr_mul(result->imag, sin_real, sinh_imag, rnd);
    mpfr_neg(result->imag, result->imag, rnd);
    result->is_complex = arg->is_complex;
    check_complex_result(parser, result, "Function result is undefined");

    mpfr_clear(cosh_imag);
    mpfr_clear(sinh_imag);
    mpfr_clear(cos_real);
    mpfr_clear(sin_real);
}

static void complex_sqrt(MpfrParser* parser, const MpfrComplex* arg, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    mpfr_t magnitude;
    mpfr_t temp;

    mpfr_init2(magnitude, precision);
    mpfr_init2(temp, precision);

    mpfr_hypot(magnitude, arg->real, arg->imag, rnd);

    mpfr_add(temp, magnitude, arg->real, rnd);
    mpfr_div_ui(temp, temp, 2, rnd);
    mpfr_sqrt(result->real, temp, rnd);

    mpfr_sub(temp, magnitude, arg->real, rnd);
    mpfr_div_ui(temp, temp, 2, rnd);
    mpfr_sqrt(result->imag, temp, rnd);
    if (mpfr_sgn(arg->imag) < 0) {
        mpfr_neg(result->imag, result->imag, rnd);
    }

    result->is_complex = arg->is_complex || !mpfr_zero_p(result->imag);
    check_complex_result(parser, result, "Function result is undefined");

    mpfr_clear(temp);
    mpfr_clear(magnitude);
}

static int complex_pow_integer(MpfrParser* parser,
                               MpfrComplex* result,
                               const MpfrComplex* base,
                               long exponent) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);
    unsigned long magnitude = exponent < 0
                                  ? (unsigned long)(-(exponent + 1)) + 1
                                  : (unsigned long)exponent;
    MpfrComplex accumulator;
    MpfrComplex factor;
    MpfrComplex product;
    MpfrComplex one;

    complex_init(&accumulator, precision);
    complex_init(&factor, precision);
    complex_init(&product, precision);
    complex_init(&one, precision);
    complex_set_ui(&accumulator, 1, rnd);
    complex_copy(&factor, base, rnd);

    while (magnitude > 0 && parser->result->status == CALCULATOR_OK) {
        if ((magnitude & 1UL) != 0) {
            complex_mul(parser, &product, &accumulator, &factor);
            if (parser->result->status == CALCULATOR_OK) {
                complex_copy(&accumulator, &product, rnd);
            }
        }
        magnitude >>= 1;
        if (magnitude > 0) {
            complex_mul(parser, &product, &factor, &factor);
            if (parser->result->status == CALCULATOR_OK) {
                complex_copy(&factor, &product, rnd);
            }
        }
    }

    if (parser->result->status == CALCULATOR_OK) {
        if (exponent < 0) {
            complex_set_ui(&one, 1, rnd);
            complex_div(parser, result, &one, &accumulator);
        } else {
            complex_copy(result, &accumulator, rnd);
        }
    }

    complex_clear(&one);
    complex_clear(&product);
    complex_clear(&factor);
    complex_clear(&accumulator);
    return parser->result->status == CALCULATOR_OK;
}

static void complex_pow(MpfrParser* parser,
                        MpfrComplex* result,
                        const MpfrComplex* base,
                        const MpfrComplex* exponent) {
    const mpfr_rnd_t rnd = parser_rounding(parser);
    const mpfr_prec_t precision = complex_precision(result);

    if (!base->is_complex && !exponent->is_complex &&
        (mpfr_sgn(base->real) >= 0 || mpfr_integer_p(exponent->real))) {
        mpfr_pow(result->real, base->real, exponent->real, rnd);
        mpfr_set_zero(result->imag, 0);
        result->is_complex = 0;
        check_complex_result(parser, result, "Power result is undefined");
        return;
    }

    if (!exponent->is_complex &&
        mpfr_integer_p(exponent->real) &&
        mpfr_fits_slong_p(exponent->real, MPFR_RNDN)) {
        complex_pow_integer(parser, result, base, mpfr_get_si(exponent->real, MPFR_RNDN));
        return;
    }

    if (mpfr_zero_p(base->real) && mpfr_zero_p(base->imag)) {
        if (!exponent->is_complex && mpfr_sgn(exponent->real) > 0) {
            complex_set_ui(result, 0, rnd);
        } else if (!exponent->is_complex && mpfr_zero_p(exponent->real)) {
            complex_set_ui(result, 1, rnd);
        } else {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "Power result is undefined");
            complex_set_nan(result);
        }
        return;
    }

    MpfrComplex logarithm;
    MpfrComplex product;
    complex_init(&logarithm, precision);
    complex_init(&product, precision);

    complex_ln(parser, base, &logarithm);
    if (parser->result->status == CALCULATOR_OK) {
        complex_mul(parser, &product, exponent, &logarithm);
    }
    if (parser->result->status == CALCULATOR_OK) {
        complex_exp(parser, &product, result);
    }

    complex_clear(&product);
    complex_clear(&logarithm);
}

static void apply_function(MpfrParser* parser,
                           const char* name,
                           const MpfrComplex* arg,
                           MpfrComplex* value) {
    const mpfr_rnd_t rnd = parser_rounding(parser);

    if (strcmp(name, "deg") == 0 || strcmp(name, "degrees") == 0) {
        mpfr_t pi;
        mpfr_init2(pi, complex_precision(value));
        mpfr_const_pi(pi, rnd);
        mpfr_mul(value->real, arg->real, pi, rnd);
        mpfr_mul(value->imag, arg->imag, pi, rnd);
        mpfr_div_ui(value->real, value->real, 180, rnd);
        mpfr_div_ui(value->imag, value->imag, 180, rnd);
        value->is_complex = arg->is_complex;
        check_complex_result(parser, value, "Function result is undefined");
        mpfr_clear(pi);
        return;
    }
    if (strcmp(name, "sqrt") == 0) {
        complex_sqrt(parser, arg, value);
        return;
    }
    if (strcmp(name, "sin") == 0) {
        complex_sin(parser, arg, value);
        return;
    }
    if (strcmp(name, "cos") == 0) {
        complex_cos(parser, arg, value);
        return;
    }
    if (strcmp(name, "tan") == 0) {
        MpfrComplex sin_value;
        MpfrComplex cos_value;
        complex_init(&sin_value, complex_precision(value));
        complex_init(&cos_value, complex_precision(value));
        complex_sin(parser, arg, &sin_value);
        if (parser->result->status == CALCULATOR_OK) {
            complex_cos(parser, arg, &cos_value);
        }
        if (parser->result->status == CALCULATOR_OK) {
            complex_div(parser, value, &sin_value, &cos_value);
        }
        complex_clear(&cos_value);
        complex_clear(&sin_value);
        return;
    }
    if (strcmp(name, "log") == 0) {
        complex_ln(parser, arg, value);
        if (parser->result->status == CALCULATOR_OK) {
            mpfr_t log10;
            mpfr_init2(log10, complex_precision(value));
            mpfr_set_ui(log10, 10, rnd);
            mpfr_log(log10, log10, rnd);
            mpfr_div(value->real, value->real, log10, rnd);
            mpfr_div(value->imag, value->imag, log10, rnd);
            complex_normalize(value);
            check_complex_result(parser, value, "Function result is undefined");
            mpfr_clear(log10);
        }
        return;
    }
    if (strcmp(name, "ln") == 0) {
        complex_ln(parser, arg, value);
        return;
    }
    if (strcmp(name, "abs") == 0) {
        complex_abs_value(parser, arg, value);
        return;
    }
    if (strcmp(name, "exp") == 0) {
        complex_exp(parser, arg, value);
        return;
    }

    set_error(parser, CALCULATOR_ERROR_SYNTAX, "Unknown function");
}

static int get_constant(MpfrParser* parser, const char* name, MpfrComplex* value) {
    const mpfr_rnd_t rnd = parser_rounding(parser);

    if (strcmp(name, "i") == 0) {
        mpfr_set_zero(value->real, 0);
        mpfr_set_ui(value->imag, 1, rnd);
        value->is_complex = 1;
        return 1;
    }
    if (strcmp(name, "pi") == 0) {
        mpfr_const_pi(value->real, rnd);
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        return 1;
    }
    if (strcmp(name, "c") == 0) {
        mpfr_set_str(value->real, PHYSICS_C, 10, rnd);
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        return 1;
    }
    if (strcmp(name, "G") == 0) {
        mpfr_set_str(value->real, PHYSICS_G, 10, rnd);
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        return 1;
    }
    if (strcmp(name, "h") == 0) {
        mpfr_set_str(value->real, PHYSICS_H, 10, rnd);
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        return 1;
    }
    if (strcmp(name, "k") == 0) {
        mpfr_set_str(value->real, PHYSICS_K, 10, rnd);
        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        return 1;
    }

    return 0;
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

static int get_context_symbol(MpfrParser* parser, const char* name, MpfrComplex* value) {
    size_t index = 0;
    if (!find_symbol_index(parser->context, name, &index)) {
        return 0;
    }

    const CalculatorMpfrSymbol* symbol = &parser->context->symbols[index];
    mpfr_set(value->real, symbol->value, parser->context->rounding);
    mpfr_set(value->imag, symbol->imaginary_value, parser->context->rounding);
    value->is_complex = symbol->is_complex;
    complex_normalize(value);

    if (parser->auto_update_var && symbol->kind == CALCULATOR_SYMBOL_VAR) {
        if (!parser->has_mutable_reference) {
            parser->has_mutable_reference = 1;
            parser->mutable_reference_index = index;
        } else if (parser->mutable_reference_index != index) {
            parser->has_multiple_mutable_references = 1;
        }
    }

    return 1;
}

static void parse_unary(MpfrParser* parser, MpfrComplex* value);

static void apply_factorial(MpfrParser* parser, const MpfrComplex* value, MpfrComplex* result) {
    const mpfr_rnd_t rnd = parser_rounding(parser);

    if (value->is_complex) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Factorial argument must be real");
        complex_set_nan(result);
        return;
    }
    if (!mpfr_number_p(value->real)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Factorial argument must be finite");
        complex_set_nan(result);
        return;
    }
    if (mpfr_sgn(value->real) < 0 || !mpfr_integer_p(value->real)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Factorial argument must be a non-negative integer");
        complex_set_nan(result);
        return;
    }

    mpfr_t gamma_arg;
    mpfr_init2(gamma_arg, complex_precision(result));
    mpfr_add_ui(gamma_arg, value->real, 1, rnd);
    mpfr_gamma(result->real, gamma_arg, rnd);
    mpfr_set_zero(result->imag, 0);
    result->is_complex = 0;
    mpfr_clear(gamma_arg);
    check_complex_result(parser, result, "Factorial result is undefined");
}

static void parse_primary(MpfrParser* parser, MpfrComplex* value) {
    if (!enter(parser)) {
        complex_set_nan(value);
        return;
    }

    const MpfrToken token = parser->lexer.current;

    if (accept(parser, MPFR_TOKEN_NUMBER)) {
        char* number = malloc(token.number_length + 1);
        if (number == NULL) {
            set_error(parser, CALCULATOR_ERROR_MEMORY, "Cannot allocate number text");
            leave(parser);
            return;
        }

        memcpy(number, token.number_start, token.number_length);
        number[token.number_length] = '\0';
        if (mpfr_set_str(value->real, number, 10, parser_rounding(parser)) != 0) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Invalid number");
        }
        free(number);

        mpfr_set_zero(value->imag, 0);
        value->is_complex = 0;
        if (parser->result->status == CALCULATOR_OK && is_imaginary_unit_token(&parser->lexer.current)) {
            mpfr_set(value->imag, value->real, parser_rounding(parser));
            mpfr_set_zero(value->real, 0);
            value->is_complex = 1;
            mpfr_lexer_next(&parser->lexer);
        }
    } else if (accept(parser, MPFR_TOKEN_LPAREN)) {
        mpfr_parse_expression(parser, value);
        if (parser->result->status == CALCULATOR_OK && !accept(parser, MPFR_TOKEN_RPAREN)) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected ')'");
        }
    } else if (token.type == MPFR_TOKEN_IDENTIFIER) {
        char identifier[CALCULATOR_MPFR_IDENTIFIER_SIZE];
        strcpy(identifier, token.text);
        mpfr_lexer_next(&parser->lexer);

        if (accept(parser, MPFR_TOKEN_LPAREN)) {
            MpfrComplex arg;
            complex_init(&arg, complex_precision(value));
            mpfr_parse_expression(parser, &arg);
            if (parser->result->status == CALCULATOR_OK && !accept(parser, MPFR_TOKEN_RPAREN)) {
                set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected ')' after function argument");
            }
            if (parser->result->status == CALCULATOR_OK) {
                apply_function(parser, identifier, &arg, value);
            }
            complex_clear(&arg);
        } else if (!get_constant(parser, identifier, value) &&
                   !get_context_symbol(parser, identifier, value)) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Unknown identifier");
        }
    } else {
        set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected a number, function, or '('");
    }

    if (parser->result->status == CALCULATOR_OK) {
        check_complex_result(parser, value, "Expression result is undefined");
    }
    leave(parser);
}

static void parse_postfix(MpfrParser* parser, MpfrComplex* value) {
    parse_primary(parser, value);

    while (parser->result->status == CALCULATOR_OK && accept(parser, MPFR_TOKEN_BANG)) {
        MpfrComplex result;
        complex_init(&result, complex_precision(value));
        apply_factorial(parser, value, &result);
        if (parser->result->status == CALCULATOR_OK) {
            complex_copy(value, &result, parser_rounding(parser));
        }
        complex_clear(&result);
    }
}

static void parse_power(MpfrParser* parser, MpfrComplex* value) {
    parse_postfix(parser, value);
    if (parser->result->status != CALCULATOR_OK) {
        return;
    }

    if (accept(parser, MPFR_TOKEN_CARET)) {
        MpfrComplex right;
        MpfrComplex result;
        complex_init(&right, complex_precision(value));
        complex_init(&result, complex_precision(value));
        parse_unary(parser, &right);
        if (parser->result->status == CALCULATOR_OK) {
            complex_pow(parser, &result, value, &right);
        }
        if (parser->result->status == CALCULATOR_OK) {
            complex_copy(value, &result, parser_rounding(parser));
        }
        complex_clear(&result);
        complex_clear(&right);
    }
}

static void parse_unary(MpfrParser* parser, MpfrComplex* value) {
    if (accept(parser, MPFR_TOKEN_PLUS)) {
        parse_unary(parser, value);
        return;
    }
    if (accept(parser, MPFR_TOKEN_MINUS)) {
        parse_unary(parser, value);
        complex_neg(parser, value);
        return;
    }
    parse_power(parser, value);
}

static void parse_factor(MpfrParser* parser, MpfrComplex* value) {
    MpfrComplex right;
    MpfrComplex result;

    parse_unary(parser, value);
    complex_init(&right, complex_precision(value));
    complex_init(&result, complex_precision(value));

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == MPFR_TOKEN_STAR || parser->lexer.current.type == MPFR_TOKEN_SLASH)) {
        const MpfrTokenType op = parser->lexer.current.type;
        mpfr_lexer_next(&parser->lexer);
        parse_unary(parser, &right);

        if (parser->result->status != CALCULATOR_OK) {
            break;
        }
        if (op == MPFR_TOKEN_STAR) {
            complex_mul(parser, &result, value, &right);
        } else {
            complex_div(parser, &result, value, &right);
        }
        if (parser->result->status == CALCULATOR_OK) {
            complex_copy(value, &result, parser_rounding(parser));
        }
    }

    complex_clear(&result);
    complex_clear(&right);
}

void mpfr_parse_expression(MpfrParser* parser, MpfrComplex* value) {
    MpfrComplex right;

    parse_factor(parser, value);
    complex_init(&right, complex_precision(value));

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == MPFR_TOKEN_PLUS || parser->lexer.current.type == MPFR_TOKEN_MINUS)) {
        const MpfrTokenType op = parser->lexer.current.type;
        mpfr_lexer_next(&parser->lexer);
        parse_factor(parser, &right);

        if (parser->result->status != CALCULATOR_OK) {
            break;
        }
        if (op == MPFR_TOKEN_PLUS) {
            complex_add(parser, value, &right);
        } else {
            complex_sub(parser, value, &right);
        }
    }

    complex_clear(&right);
}
