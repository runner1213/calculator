#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "math_parser.h"

void print_help(void) {
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
    printf("\nExamples:\n");
    printf("  sqrt(25)             = 5\n");
    printf("  2 + 3 * 4           = 14\n");
    printf("  (2 + 3) * 4         = 20\n");
    printf("  2^3 + 1             = 9\n");
    printf("  sin(0) + cos(0)     = 1\n");
    printf("  sqrt(2^4 + 3^2)     = 5\n");
    printf("  log(100)            = 2\n");
    printf("\nSpecial commands:\n");
    printf("  help - show this help\n");
    printf("  exit - quit program\n");
    printf("============================\n\n");
}

char* read_dynamic_line(void) {
    size_t capacity = 100;
    size_t size = 0;
    char* buffer = (char*)malloc(capacity);

    if (buffer == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }

    while (1) {
        if (fgets(buffer + size, capacity - size, stdin) == NULL) {
            free(buffer);
            return NULL;
        }

        size_t read_len = strlen(buffer + size);
        size += read_len;

        if (size > 0 && buffer[size - 1] == '\n') {
            buffer[size - 1] = '\0';
            break;
        }

        if (size + 1 >= capacity) {
            capacity *= 2;
            char* new_buffer = (char*)realloc(buffer, capacity);
            if (new_buffer == NULL) {
                printf("Memory reallocation failed\n");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    return buffer;
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

        if (strlen(expression) == 0) {
            free(expression);
            continue;
        }

        double result = parser(expression);

        if (isnan(result)) {
            printf("Error: Could not calculate expression\n");
        } else {
            printf("Result: %f\n", result);
        }

        free(expression);
    }

    printf("Goodbye!\n");
    return 0;
}