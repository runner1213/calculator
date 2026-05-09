#include "lexer.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void set_error(CalculatorResult* result, CalculatorStatus status, const char* message) {
    if (result->status != CALCULATOR_OK) {
        return;
    }

    result->status = status;
    strncpy(result->error, message, CALCULATOR_ERROR_SIZE - 1);
    result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
}

void lexer_init(Lexer* lexer, const char* input, CalculatorResult* result) {
    lexer->input = input;
    lexer->position = 0;
    lexer->result = result;
    if ((unsigned char)input[0] == 0xEF &&
        (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        lexer->position = 3;
    }
    lexer_next(lexer);
}

void lexer_next(Lexer* lexer) {
    const char* input = lexer->input;
    size_t pos = lexer->position;

    while (isspace((unsigned char)input[pos])) {
        pos++;
    }

    lexer->current.position = pos;
    lexer->current.number = 0.0;
    lexer->current.text[0] = '\0';

    const char c = input[pos];
    if (c == '\0') {
        lexer->current.type = TOKEN_END;
        lexer->position = pos;
        return;
    }

    if (isdigit((unsigned char)c) || c == '.') {
        char* end = NULL;
        errno = 0;
        lexer->current.number = strtod(input + pos, &end);
        if (end == input + pos) {
            lexer->current.type = TOKEN_INVALID;
            set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Invalid number");
            return;
        }
        if (errno == ERANGE) {
            lexer->current.type = TOKEN_INVALID;
            set_error(lexer->result, CALCULATOR_ERROR_OVERFLOW, "Number is outside supported range");
            return;
        }

        lexer->current.type = TOKEN_NUMBER;
        lexer->position = (size_t)(end - input);
        return;
    }

    if (isalpha((unsigned char)c)) {
        size_t start = pos;
        size_t len = 0;
        while (isalpha((unsigned char)input[pos])) {
            if (len + 1 >= CALCULATOR_IDENTIFIER_SIZE) {
                lexer->current.type = TOKEN_INVALID;
                set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Identifier is too long");
                return;
            }
            lexer->current.text[len++] = input[pos++];
        }
        lexer->current.text[len] = '\0';
        lexer->current.position = start;
        lexer->current.type = TOKEN_IDENTIFIER;
        lexer->position = pos;
        return;
    }

    lexer->position = pos + 1;
    switch (c) {
        case '+': lexer->current.type = TOKEN_PLUS; return;
        case '-': lexer->current.type = TOKEN_MINUS; return;
        case '*': lexer->current.type = TOKEN_STAR; return;
        case '/': lexer->current.type = TOKEN_SLASH; return;
        case '^': lexer->current.type = TOKEN_CARET; return;
        case '(': lexer->current.type = TOKEN_LPAREN; return;
        case ')': lexer->current.type = TOKEN_RPAREN; return;
        case ',': lexer->current.type = TOKEN_COMMA; return;
        default:
            lexer->current.type = TOKEN_INVALID;
            set_error(lexer->result, CALCULATOR_ERROR_SYNTAX, "Unknown character");
            return;
    }
}
