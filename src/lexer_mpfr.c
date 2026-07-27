#include "lexer_mpfr.h"

#include <ctype.h>
#include <string.h>

static void set_error(CalculatorMpfrResult* result, CalculatorStatus status, const char* message) {
    if (result->status != CALCULATOR_OK) {
        return;
    }

    result->status = status;
    strncpy(result->error, message, CALCULATOR_ERROR_SIZE - 1);
    result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
}

static int is_identifier_start(unsigned char c) {
    return c == '_' || isalpha(c) || c >= 0x80;
}

static int is_identifier_part(unsigned char c) {
    return c == '_' || isalnum(c) || c >= 0x80;
}

void mpfr_lexer_init(MpfrLexer* lexer, const char* input, CalculatorMpfrResult* result) {
    lexer->input = input;
    lexer->position = 0;
    lexer->result = result;
    if ((unsigned char)input[0] == 0xEF &&
        (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        lexer->position = 3;
    }
    mpfr_lexer_next(lexer);
}

void mpfr_lexer_next(MpfrLexer* lexer) {
    const char* input = lexer->input;
    size_t pos = lexer->position;

    while (isspace((unsigned char)input[pos])) {
        pos++;
    }

    lexer->current.position = pos;
    lexer->current.number_start = NULL;
    lexer->current.number_length = 0;
    lexer->current.text[0] = '\0';

    const char c = input[pos];
    if (c == '\0') {
        lexer->current.type = MPFR_TOKEN_END;
        lexer->position = pos;
        return;
    }

    if (isdigit((unsigned char)c) || c == '.') {
        const size_t start = pos;
        int has_digits = 0;

        while (isdigit((unsigned char)input[pos])) {
            pos++;
            has_digits = 1;
        }
        if (input[pos] == '.') {
            pos++;
            while (isdigit((unsigned char)input[pos])) {
                pos++;
                has_digits = 1;
            }
        }
        if (!has_digits) {
            lexer->current.type = MPFR_TOKEN_INVALID;
            set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Invalid number");
            return;
        }
        if (input[pos] == 'e' || input[pos] == 'E') {
            const size_t exponent_start = pos;
            int has_exponent_digits = 0;
            pos++;
            if (input[pos] == '+' || input[pos] == '-') {
                pos++;
            }
            while (isdigit((unsigned char)input[pos])) {
                pos++;
                has_exponent_digits = 1;
            }
            if (!has_exponent_digits) {
                lexer->current.type = MPFR_TOKEN_INVALID;
                lexer->position = exponent_start;
                set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Invalid number exponent");
                return;
            }
        }

        lexer->current.type = MPFR_TOKEN_NUMBER;
        lexer->current.number_start = input + start;
        lexer->current.number_length = pos - start;
        lexer->position = pos;
        return;
    }

    if (is_identifier_start((unsigned char)c)) {
        size_t start = pos;
        size_t len = 0;
        while (is_identifier_part((unsigned char)input[pos])) {
            if (len + 1 >= CALCULATOR_MPFR_IDENTIFIER_SIZE) {
                lexer->current.type = MPFR_TOKEN_INVALID;
                set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Identifier is too long");
                return;
            }
            lexer->current.text[len++] = input[pos++];
        }
        lexer->current.text[len] = '\0';
        lexer->current.position = start;
        lexer->current.type = MPFR_TOKEN_IDENTIFIER;
        lexer->position = pos;
        return;
    }

    lexer->position = pos + 1;
    switch (c) {
        case '+': lexer->current.type = MPFR_TOKEN_PLUS; return;
        case '-': lexer->current.type = MPFR_TOKEN_MINUS; return;
        case '*': lexer->current.type = MPFR_TOKEN_STAR; return;
        case '/': lexer->current.type = MPFR_TOKEN_SLASH; return;
        case '^': lexer->current.type = MPFR_TOKEN_CARET; return;
        case '(': lexer->current.type = MPFR_TOKEN_LPAREN; return;
        case ')': lexer->current.type = MPFR_TOKEN_RPAREN; return;
        case '!': lexer->current.type = MPFR_TOKEN_BANG; return;
        case ',': lexer->current.type = MPFR_TOKEN_COMMA; return;
        case '=': lexer->current.type = MPFR_TOKEN_EQUAL; return;
        default:
            lexer->current.type = MPFR_TOKEN_INVALID;
            set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Unknown character");
            return;
    }
}
