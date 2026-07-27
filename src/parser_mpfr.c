#include "parser_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARSE_DEPTH 512
#define PHYSICS_C "299792458"
#define PHYSICS_G "6.67430e-11"
#define PHYSICS_H "6.62607015e-34"
#define PHYSICS_K "1.380649e-23"

static void set_error(MpfrParser* parser, CalculatorStatus status, const char* message) {
    if (parser->result->status != CALCULATOR_OK) {
        return;
    }

    parser->result->status = status;
    snprintf(parser->result->error, CALCULATOR_ERROR_SIZE, "%s at position %zu",
             message, parser->lexer.current.position);
    mpfr_set_nan(parser->result->value);
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

static void apply_function(MpfrParser* parser, const char* name, const mpfr_t arg, mpfr_t value) {
    const mpfr_rnd_t rnd = parser->context != NULL ? parser->context->rounding : MPFR_RNDN;

    if (strcmp(name, "deg") == 0 || strcmp(name, "degrees") == 0) {
        mpfr_t pi;
        mpfr_init2(pi, mpfr_get_prec(value));
        mpfr_const_pi(pi, rnd);
        mpfr_mul(value, arg, pi, rnd);
        mpfr_div_ui(value, value, 180, rnd);
        mpfr_clear(pi);
        return;
    }
    if (strcmp(name, "sqrt") == 0) {
        if (mpfr_sgn(arg) < 0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "sqrt() argument must be non-negative");
            return;
        }
        mpfr_sqrt(value, arg, rnd);
        return;
    }
    if (strcmp(name, "sin") == 0) {
        mpfr_sin(value, arg, rnd);
        return;
    }
    if (strcmp(name, "cos") == 0) {
        mpfr_cos(value, arg, rnd);
        return;
    }
    if (strcmp(name, "tan") == 0) {
        mpfr_tan(value, arg, rnd);
        return;
    }
    if (strcmp(name, "log") == 0) {
        if (mpfr_sgn(arg) <= 0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "log() argument must be positive");
            return;
        }
        mpfr_log10(value, arg, rnd);
        return;
    }
    if (strcmp(name, "ln") == 0) {
        if (mpfr_sgn(arg) <= 0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "ln() argument must be positive");
            return;
        }
        mpfr_log(value, arg, rnd);
        return;
    }
    if (strcmp(name, "abs") == 0) {
        mpfr_abs(value, arg, rnd);
        return;
    }
    if (strcmp(name, "exp") == 0) {
        mpfr_exp(value, arg, rnd);
        return;
    }

    set_error(parser, CALCULATOR_ERROR_SYNTAX, "Unknown function");
}

static int get_constant(MpfrParser* parser, const char* name, mpfr_t value) {
    const mpfr_rnd_t rnd = parser->context != NULL ? parser->context->rounding : MPFR_RNDN;

    if (strcmp(name, "pi") == 0) {
        mpfr_const_pi(value, rnd);
        return 1;
    }
    if (strcmp(name, "c") == 0) {
        mpfr_set_str(value, PHYSICS_C, 10, rnd);
        return 1;
    }
    if (strcmp(name, "G") == 0) {
        mpfr_set_str(value, PHYSICS_G, 10, rnd);
        return 1;
    }
    if (strcmp(name, "h") == 0) {
        mpfr_set_str(value, PHYSICS_H, 10, rnd);
        return 1;
    }
    if (strcmp(name, "k") == 0) {
        mpfr_set_str(value, PHYSICS_K, 10, rnd);
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

static int get_context_symbol(MpfrParser* parser, const char* name, mpfr_t value) {
    size_t index = 0;
    if (!find_symbol_index(parser->context, name, &index)) {
        return 0;
    }

    const CalculatorMpfrSymbol* symbol = &parser->context->symbols[index];
    mpfr_set(value, symbol->value, parser->context->rounding);

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

static void parse_unary(MpfrParser* parser, mpfr_t value);

static void apply_factorial(MpfrParser* parser, const mpfr_t value, mpfr_t result) {
    const mpfr_rnd_t rnd = parser->context != NULL ? parser->context->rounding : MPFR_RNDN;

    if (!mpfr_number_p(value)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Factorial argument must be finite");
        return;
    }
    if (mpfr_sgn(value) < 0 || !mpfr_integer_p(value)) {
        set_error(parser, CALCULATOR_ERROR_DOMAIN, "Factorial argument must be a non-negative integer");
        return;
    }

    mpfr_t gamma_arg;
    mpfr_init2(gamma_arg, mpfr_get_prec(result));
    mpfr_add_ui(gamma_arg, value, 1, rnd);
    mpfr_gamma(result, gamma_arg, rnd);
    mpfr_clear(gamma_arg);
    check_number_result(parser, result, "Factorial result is undefined");
}

static void parse_primary(MpfrParser* parser, mpfr_t value) {
    if (!enter(parser)) {
        mpfr_set_nan(value);
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
        if (mpfr_set_str(value, number, 10, parser->context != NULL ? parser->context->rounding : MPFR_RNDN) != 0) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Invalid number");
        }
        free(number);
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
            mpfr_t arg;
            mpfr_init2(arg, mpfr_get_prec(value));
            mpfr_parse_expression(parser, arg);
            if (parser->result->status == CALCULATOR_OK && !accept(parser, MPFR_TOKEN_RPAREN)) {
                set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected ')' after function argument");
            }
            if (parser->result->status == CALCULATOR_OK) {
                apply_function(parser, identifier, arg, value);
                if (parser->result->status == CALCULATOR_OK) {
                    check_number_result(parser, value, "Function result is undefined");
                }
            }
            mpfr_clear(arg);
        } else if (!get_constant(parser, identifier, value) &&
                   !get_context_symbol(parser, identifier, value)) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Unknown identifier");
        }
    } else {
        set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected a number, function, or '('");
    }

    leave(parser);
}

static void parse_postfix(MpfrParser* parser, mpfr_t value) {
    parse_primary(parser, value);

    while (parser->result->status == CALCULATOR_OK && accept(parser, MPFR_TOKEN_BANG)) {
        apply_factorial(parser, value, value);
    }
}

static void parse_power(MpfrParser* parser, mpfr_t value) {
    parse_postfix(parser, value);
    if (parser->result->status != CALCULATOR_OK) {
        return;
    }

    if (accept(parser, MPFR_TOKEN_CARET)) {
        mpfr_t right;
        mpfr_init2(right, mpfr_get_prec(value));
        parse_unary(parser, right);
        if (parser->result->status == CALCULATOR_OK) {
            mpfr_pow(value, value, right, parser->context != NULL ? parser->context->rounding : MPFR_RNDN);
            check_number_result(parser, value, "Power result is undefined");
        }
        mpfr_clear(right);
    }
}

static void parse_unary(MpfrParser* parser, mpfr_t value) {
    if (accept(parser, MPFR_TOKEN_PLUS)) {
        parse_unary(parser, value);
        return;
    }
    if (accept(parser, MPFR_TOKEN_MINUS)) {
        parse_unary(parser, value);
        mpfr_neg(value, value, parser->context != NULL ? parser->context->rounding : MPFR_RNDN);
        return;
    }
    parse_power(parser, value);
}

static void parse_factor(MpfrParser* parser, mpfr_t value) {
    const mpfr_rnd_t rnd = parser->context != NULL ? parser->context->rounding : MPFR_RNDN;
    mpfr_t right;

    parse_unary(parser, value);
    mpfr_init2(right, mpfr_get_prec(value));

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == MPFR_TOKEN_STAR || parser->lexer.current.type == MPFR_TOKEN_SLASH)) {
        const MpfrTokenType op = parser->lexer.current.type;
        mpfr_lexer_next(&parser->lexer);
        parse_unary(parser, right);

        if (parser->result->status != CALCULATOR_OK) {
            break;
        }
        if (op == MPFR_TOKEN_STAR) {
            mpfr_mul(value, value, right, rnd);
        } else {
            if (mpfr_zero_p(right)) {
                set_error(parser, CALCULATOR_ERROR_DOMAIN, "Division by zero");
                break;
            }
            mpfr_div(value, value, right, rnd);
        }
        check_number_result(parser, value, "Arithmetic result is undefined");
    }

    mpfr_clear(right);
}

void mpfr_parse_expression(MpfrParser* parser, mpfr_t value) {
    const mpfr_rnd_t rnd = parser->context != NULL ? parser->context->rounding : MPFR_RNDN;
    mpfr_t right;

    parse_factor(parser, value);
    mpfr_init2(right, mpfr_get_prec(value));

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == MPFR_TOKEN_PLUS || parser->lexer.current.type == MPFR_TOKEN_MINUS)) {
        const MpfrTokenType op = parser->lexer.current.type;
        mpfr_lexer_next(&parser->lexer);
        parse_factor(parser, right);

        if (parser->result->status != CALCULATOR_OK) {
            break;
        }
        if (op == MPFR_TOKEN_PLUS) {
            mpfr_add(value, value, right, rnd);
        } else {
            mpfr_sub(value, value, right, rnd);
        }
        check_number_result(parser, value, "Arithmetic result is undefined");
    }

    mpfr_clear(right);
}
