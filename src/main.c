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
    printf("  c         - speed of light in vacuum, m/s\n");
    printf("  G         - gravitational constant, N*m^2/kg^2\n");
    printf("  h         - Planck constant, J*s\n");
    printf("  k         - Boltzmann constant, J/K\n");
    printf("\nSession symbols:\n");
    printf("  const name = expression  - set a constant value\n");
    printf("  const name = null        - delete a constant value\n");
    printf("  var name = expression    - set a mutable accumulator value\n");
    printf("  var name = null          - delete a mutable accumulator value\n");
    printf("  name = expression        - shorthand for const name = expression\n");
    printf("\nExamples:\n");
    printf("  sqrt(25)             = 5\n");
    printf("  2 + 3 * 4            = 14\n");
    printf("  (2 + 3) * 4          = 20\n");
    printf("  2^3 + 1              = 9\n");
    printf("  sin(0) + cos(0)      = 1\n");
    printf("  sin(deg(90))         = 1\n");
    printf("  c^2                  = 8.987551787e16\n");
    printf("  cos(pi)              = -1\n");
    printf("  sqrt(2^4 + 3^2)      = 5\n");
    printf("  log(100)             = 2\n");
    printf("  const rate = 6.09    - save session constant\n");
    printf("  369 / rate           = 60.5911330049261\n");
    printf("  var total = 10       - save session accumulator\n");
    printf("  total + 5            = 15, then total becomes 15\n");
    printf("\nSpecial commands:\n");
    printf("  help - show this help\n");
    printf("  exit - quit program\n");
    printf("============================\n\n");
}

static void print_result(const CalculatorResult* result) {
    if (result->status != CALCULATOR_OK) {
        printf("Error: %s\n", result->error);
        return;
    }

    switch (result->kind) {
        case CALCULATOR_RESULT_CONST_SET:
            printf("Set const: %s = %.15g\n", result->name, result->value);
            break;
        case CALCULATOR_RESULT_CONST_DELETED:
            printf("Deleted const: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_SET:
            printf("Set var: %s = %.15g\n", result->name, result->value);
            break;
        case CALCULATOR_RESULT_VAR_DELETED:
            printf("Deleted var: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_UPDATED:
            printf("Result: %.15g\n", result->value);
            printf("Updated var: %s = %.15g\n", result->name, result->value);
            break;
        case CALCULATOR_RESULT_VALUE:
        default:
            printf("Result: %.15g\n", result->value);
            break;
    }
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

    CalculatorContext context;
    calculator_context_init(&context);

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

        const CalculatorResult result = calculator_context_evaluate(&context, expression);
        print_result(&result);

        free(expression);
    }

    calculator_context_free(&context);
    printf("Goodbye!\n");
    return 0;
}
