#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calculator/calculator.h"
#include "calculator/calculator_mpfr.h"

#define DEFAULT_DECIMAL_DIGITS 15

static void print_help(void) {
    printf("\n=== MATH CALCULATOR HELP ===\n");
    printf("Supported operations:\n");
    printf("  Basic: +  -  *  /\n");
    printf("  Power: ^ (example: 2^3 = 8)\n");
    printf("  Factorial: ! (example: 5! = 120)\n");
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
    printf("  (2 + 3)!             = 120\n");
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
    printf("\nPrecision suffix:\n");
    printf("  expression:digits - print with digits after decimal point\n");
    printf("  pi:80             - print pi with 80 digits after decimal point\n");
    printf("============================\n\n");
}

static void print_mpfr_value_default(const mpfr_t value) {
    mpfr_printf("%.15Rg", value);
}

static void print_mpfr_value_fixed(const CalculatorMpfrResult* result, size_t digits_after_point) {
    char* formatted = NULL;
    const CalculatorStatus status = calculator_mpfr_result_format_fixed(result, digits_after_point, &formatted);
    if (status != CALCULATOR_OK) {
        printf("<format error>");
        return;
    }

    printf("%s", formatted);
    calculator_mpfr_free_string(formatted);
}

static void print_mpfr_value(const CalculatorMpfrResult* result, int fixed_digits, size_t digits_after_point) {
    if (fixed_digits) {
        print_mpfr_value_fixed(result, digits_after_point);
    } else {
        print_mpfr_value_default(result->value);
    }
}

static void print_result(const CalculatorMpfrResult* result, int fixed_digits, size_t digits_after_point) {
    if (result->status != CALCULATOR_OK) {
        printf("Error: %s\n", result->error);
        return;
    }

    switch (result->kind) {
        case CALCULATOR_RESULT_CONST_SET:
            printf("Set const: %s = ", result->name);
            print_mpfr_value(result, fixed_digits, digits_after_point);
            printf("\n");
            break;
        case CALCULATOR_RESULT_CONST_DELETED:
            printf("Deleted const: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_SET:
            printf("Set var: %s = ", result->name);
            print_mpfr_value(result, fixed_digits, digits_after_point);
            printf("\n");
            break;
        case CALCULATOR_RESULT_VAR_DELETED:
            printf("Deleted var: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_UPDATED:
            printf("Result: ");
            print_mpfr_value(result, fixed_digits, digits_after_point);
            printf("\n");
            printf("Updated var: %s = ", result->name);
            print_mpfr_value(result, fixed_digits, digits_after_point);
            printf("\n");
            break;
        case CALCULATOR_RESULT_VALUE:
        default:
            printf("Result: ");
            print_mpfr_value(result, fixed_digits, digits_after_point);
            printf("\n");
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

static int parse_precision_suffix(char* expression, size_t* digits_after_point) {
    char* colon = strrchr(expression, ':');
    if (colon == NULL) {
        return 0;
    }

    char* digits = colon + 1;
    while (*digits == ' ' || *digits == '\t') {
        digits++;
    }

    if (*digits == '\0') {
        return -1;
    }

    size_t value = 0;
    while (*digits != '\0') {
        if (*digits == ' ' || *digits == '\t') {
            char* rest = digits;
            while (*rest == ' ' || *rest == '\t') {
                rest++;
            }
            if (*rest != '\0') {
                return -1;
            }
            break;
        }
        if (*digits < '0' || *digits > '9') {
            return -1;
        }

        const size_t digit = (size_t)(*digits - '0');
        if (value > (((size_t)-1) - digit) / 10) {
            return -1;
        }
        value = value * 10 + digit;
        digits++;
    }

    char* end = colon;
    while (end > expression && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';

    if (expression[0] == '\0') {
        return -1;
    }

    *digits_after_point = value;
    return 1;
}

int main(void) {
    printf("Type 'help' for available commands and functions\n");
    printf("Type 'exit' to quit\n\n");

    CalculatorMpfrContext context;
    calculator_mpfr_context_init(
        &context,
        calculator_mpfr_precision_for_decimal_digits(DEFAULT_DECIMAL_DIGITS),
        MPFR_RNDN);

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

        size_t digits_after_point = DEFAULT_DECIMAL_DIGITS;
        const int has_precision_suffix = parse_precision_suffix(expression, &digits_after_point);
        if (has_precision_suffix < 0) {
            printf("Error: invalid precision suffix\n");
            free(expression);
            continue;
        }

        calculator_mpfr_context_set_precision(
            &context,
            calculator_mpfr_precision_for_decimal_digits(digits_after_point));

        CalculatorMpfrResult result;
        calculator_mpfr_result_init(&result, context.precision);
        calculator_mpfr_context_evaluate(&context, expression, &result);
        print_result(&result, has_precision_suffix > 0, digits_after_point);
        calculator_mpfr_result_clear(&result);

        free(expression);
    }

    calculator_mpfr_context_free(&context);
    printf("Goodbye!\n");
    return 0;
}
