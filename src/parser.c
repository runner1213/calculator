#include "parser.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_PARSE_DEPTH 512
#define CALCULATOR_PI 3.14159265358979323846

static void set_error(Parser* parser, CalculatorStatus status, const char* message) {
    if (parser->result->status != CALCULATOR_OK) {
        return;
    }

    parser->result->status = status;
    snprintf(parser->result->error, CALCULATOR_ERROR_SIZE, "%s at position %zu",
             message, parser->lexer.current.position);
}

static int enter(Parser* parser) {
    if (++parser->depth > MAX_PARSE_DEPTH) {
        set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expression nesting is too deep");
        return 0;
    }
    return 1;
}

static void leave(Parser* parser) {
    if (parser->depth > 0) {
        parser->depth--;
    }
}

static int accept(Parser* parser, TokenType type) {
    if (parser->lexer.current.type != type) {
        return 0;
    }
    lexer_next(&parser->lexer);
    return 1;
}

static double apply_function(Parser* parser, const char* name, double arg) {
    if (strcmp(name, "deg") == 0 || strcmp(name, "degrees") == 0) {
        return arg * CALCULATOR_PI / 180.0;
    }
    if (strcmp(name, "sqrt") == 0) {
        if (arg < 0.0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "sqrt() argument must be non-negative");
            return NAN;
        }
        return sqrt(arg);
    }
    if (strcmp(name, "sin") == 0) return sin(arg);
    if (strcmp(name, "cos") == 0) return cos(arg);
    if (strcmp(name, "tan") == 0) return tan(arg);
    if (strcmp(name, "log") == 0) {
        if (arg <= 0.0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "log() argument must be positive");
            return NAN;
        }
        return log10(arg);
    }
    if (strcmp(name, "ln") == 0) {
        if (arg <= 0.0) {
            set_error(parser, CALCULATOR_ERROR_DOMAIN, "ln() argument must be positive");
            return NAN;
        }
        return log(arg);
    }
    if (strcmp(name, "abs") == 0) return fabs(arg);
    if (strcmp(name, "exp") == 0) return exp(arg);

    set_error(parser, CALCULATOR_ERROR_SYNTAX, "Unknown function");
    return NAN;
}

static int get_constant(const char* name, double* value) {
    if (strcmp(name, "pi") == 0) {
        *value = CALCULATOR_PI;
        return 1;
    }

    return 0;
}

static double parse_unary(Parser* parser);

static double parse_primary(Parser* parser) {
    if (!enter(parser)) {
        return NAN;
    }

    double value = NAN;
    const Token token = parser->lexer.current;

    if (accept(parser, TOKEN_NUMBER)) {
        value = token.number;
    } else if (accept(parser, TOKEN_LPAREN)) {
        value = parse_expression(parser);
        if (parser->result->status == CALCULATOR_OK && !accept(parser, TOKEN_RPAREN)) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected ')'");
        }
    } else if (token.type == TOKEN_IDENTIFIER) {
        char identifier[CALCULATOR_IDENTIFIER_SIZE];
        strcpy(identifier, token.text);
        lexer_next(&parser->lexer);

        if (accept(parser, TOKEN_LPAREN)) {
            const double arg = parse_expression(parser);
            if (parser->result->status == CALCULATOR_OK && !accept(parser, TOKEN_RPAREN)) {
                set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected ')' after function argument");
            }
            if (parser->result->status == CALCULATOR_OK) {
                value = apply_function(parser, identifier, arg);
                if (parser->result->status == CALCULATOR_OK && !isfinite(value)) {
                    set_error(parser, isnan(value) ? CALCULATOR_ERROR_DOMAIN : CALCULATOR_ERROR_OVERFLOW,
                              isnan(value) ? "Function result is undefined" : "Function result is outside supported range");
                }
            }
        } else if (!get_constant(identifier, &value)) {
            set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected '(' after function name");
        }
    } else {
        set_error(parser, CALCULATOR_ERROR_SYNTAX, "Expected a number, function, or '('");
    }

    leave(parser);
    return value;
}

static double parse_power(Parser* parser) {
    double left = parse_primary(parser);
    if (parser->result->status != CALCULATOR_OK) {
        return NAN;
    }

    if (accept(parser, TOKEN_CARET)) {
        const double right = parse_unary(parser);
        if (parser->result->status != CALCULATOR_OK) {
            return NAN;
        }
        left = pow(left, right);
        if (!isfinite(left)) {
            set_error(parser, isnan(left) ? CALCULATOR_ERROR_DOMAIN : CALCULATOR_ERROR_OVERFLOW,
                      isnan(left) ? "Power result is undefined" : "Power result is outside supported range");
            return NAN;
        }
    }

    return left;
}

static double parse_unary(Parser* parser) {
    if (accept(parser, TOKEN_PLUS)) {
        return parse_unary(parser);
    }
    if (accept(parser, TOKEN_MINUS)) {
        return -parse_unary(parser);
    }
    return parse_power(parser);
}

static double parse_factor(Parser* parser) {
    double left = parse_unary(parser);

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == TOKEN_STAR || parser->lexer.current.type == TOKEN_SLASH)) {
        const TokenType op = parser->lexer.current.type;
        lexer_next(&parser->lexer);
        const double right = parse_unary(parser);

        if (parser->result->status != CALCULATOR_OK) {
            return NAN;
        }
        if (op == TOKEN_STAR) {
            left *= right;
        } else {
            if (right == 0.0) {
                set_error(parser, CALCULATOR_ERROR_DOMAIN, "Division by zero");
                return NAN;
            }
            left /= right;
        }
        if (!isfinite(left)) {
            set_error(parser, CALCULATOR_ERROR_OVERFLOW, "Arithmetic result is outside supported range");
            return NAN;
        }
    }

    return left;
}

double parse_expression(Parser* parser) {
    double left = parse_factor(parser);

    while (parser->result->status == CALCULATOR_OK &&
           (parser->lexer.current.type == TOKEN_PLUS || parser->lexer.current.type == TOKEN_MINUS)) {
        const TokenType op = parser->lexer.current.type;
        lexer_next(&parser->lexer);
        const double right = parse_factor(parser);

        if (parser->result->status != CALCULATOR_OK) {
            return NAN;
        }
        left = (op == TOKEN_PLUS) ? left + right : left - right;
        if (!isfinite(left)) {
            set_error(parser, CALCULATOR_ERROR_OVERFLOW, "Arithmetic result is outside supported range");
            return NAN;
        }
    }

    return left;
}
