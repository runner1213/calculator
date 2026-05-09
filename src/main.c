#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calculator/calculator.h"

static void print_help(void) {
    printf("\n=== MATH CALCULATOR HELP ===\n");
    printf("Supported operations:\n");
    printf("  Basic: +  -  *  /\n");
    printf("  Power: ^ (example: 2^3 = 8)\n");
    printf("  Parentheses: ( ) for grouping\n");
    printf("\nMathematical functions:\n");
    printf("  sqrt(x)   - square root\n");
    printf("  sin(x)    - sine (radians)\n");
    printf("  cos(x)    - cosine (radians)\n");
    printf("  tan(x)    - tangent (radians)\n");
    printf("  log(x)    - base-10 logarithm\n");
    printf("  ln(x)     - natural logarithm\n");
    printf("  abs(x)    - absolute value\n");
    printf("  exp(x)    - exponential\n");
    printf("  deg(x)    - convert degrees to radians\n");
    printf("\nConstants:\n");
    printf("  pi        - 3.141592653589793...\n");
    printf("\nExamples:\n");
    printf("  sqrt(25)             = 5\n");
    printf("  2 + 3 * 4            = 14\n");
    printf("  (2 + 3) * 4          = 20\n");
    printf("  2^3 + 1              = 9\n");
    printf("  sin(0) + cos(0)      = 1\n");
    printf("  sin(deg(90))         = 1\n");
    printf("  cos(pi)              = -1\n");
    printf("  sqrt(2^4 + 3^2)      = 5\n");
    printf("  log(100)             = 2\n");
    printf("\nSpecial commands:\n");
    printf("  help - show this help\n");
    printf("  exit - quit program\n");
    printf("============================\n\n");
}

static char* read_dynamic_line(void) {
    size_t capacity = 128;
    size_t size = 0;
    char* buffer = malloc(capacity);

    if (buffer == NULL) {
        return NULL;
    }

    while (fgets(buffer + size, (int)(capacity - size), stdin) != NULL) {
        size += strlen(buffer + size);
        if (size > 0 && buffer[size - 1] == '\n') {
            buffer[size - 1] = '\0';
            return buffer;
        }

        if (capacity > ((size_t)-1) / 2) {
            free(buffer);
            return NULL;
        }

        capacity *= 2;
        char* resized = realloc(buffer, capacity);
        if (resized == NULL) {
            free(buffer);
            return NULL;
        }
        buffer = resized;
    }

    if (size > 0) {
        return buffer;
    }

    free(buffer);
    return NULL;
}

int main(void) {
    printf("Type 'help' for available commands and functions\n");
    printf("Type 'exit' to quit\n\n");

    while (1) {
        printf(">> ");

        char* expression = read_dynamic_line();
        if (expression == NULL) {
            printf("Input error\n");
            break;
        }

        if (strcmp(expression, "exit") == 0) {
            free(expression);
            break;
        }

        if (strcmp(expression, "help") == 0) {
            print_help();
            free(expression);
            continue;
        }

        if (expression[0] == '\0') {
            free(expression);
            continue;
        }

        const CalculatorResult result = calculator_evaluate(expression);
        if (result.status == CALCULATOR_OK) {
            printf("Result: %.15g\n", result.value);
        } else {
            printf("Error: %s\n", result.error);
        }

        free(expression);
    }

    printf("Goodbye!\n");
    return 0;
}
