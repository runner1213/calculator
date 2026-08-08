#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calculator/calculator.h"
#include "calculator/calculator_mpfr.h"

#define DEFAULT_DECIMAL_DIGITS 15

typedef enum {
    OUTPUT_FORMAT_GENERAL = 0,
    OUTPUT_FORMAT_SCIENTIFIC,
    OUTPUT_FORMAT_FIXED
} OutputFormat;

typedef struct {
    size_t precision_digits;
    OutputFormat format;
    size_t fixed_digits;
} CalculatorShell;

typedef enum {
    PROCESS_OK = 0,
    PROCESS_ERROR,
    PROCESS_EXIT
} ProcessLineResult;

static void print_usage(const char* program) {
    printf("Usage:\n");
    printf("  %s\n", program);
    printf("  %s -e \"expression\" [--precision digits] [--format general|scientific|fixed [digits]]\n", program);
    printf("  %s --file path [--precision digits] [--format general|scientific|fixed [digits]]\n", program);
}

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
    printf("  ans       - previous successful result\n");
    printf("  c         - speed of light in vacuum, m/s\n");
    printf("  G         - gravitational constant, N*m^2/kg^2\n");
    printf("  h         - Planck constant, J*s\n");
    printf("  k         - Boltzmann constant, J/K\n");
    printf("\nSession symbols:\n");
    printf("  const name = expression  - set a recalculated session constant\n");
    printf("  const name = null        - delete a constant value\n");
    printf("  var name = expression    - set a mutable accumulator value\n");
    printf("  var name = null          - delete a mutable accumulator value\n");
    printf("  name = expression        - shorthand for const name = expression\n");
    printf("\nExamples:\n");
    printf("  sqrt(25)             = 5\n");
    printf("  2 + 3 * 4            = 14\n");
    printf("  sin(deg(90))         = 1\n");
    printf("  const p = pi         - save pi as a recalculated constant\n");
    printf("  p:100                - print p with 100 digits after decimal point\n");
    printf("  ans * 2              - use the previous successful result\n");
    printf("  var total = 10       - save session accumulator\n");
    printf("  total + 5            = 15, then total becomes 15\n");
    printf("\nSpecial commands:\n");
    printf("  help                 - show this help\n");
    printf("  exit                 - quit program\n");
    printf("  :precision digits    - set default evaluation precision\n");
    printf("  :format general      - print significant digits\n");
    printf("  :format scientific   - print scientific notation\n");
    printf("  :format fixed digits - print fixed digits after decimal point\n");
    printf("  :vars                - list session symbols\n");
    printf("  :clear               - clear session symbols including ans\n");
    printf("\nPrecision suffix:\n");
    printf("  expression:digits    - evaluate and print this line in fixed format\n");
    printf("  pi:80                - print pi with 80 digits after decimal point\n");
    printf("============================\n\n");
}

static const char* format_name(OutputFormat format) {
    switch (format) {
        case OUTPUT_FORMAT_SCIENTIFIC:
            return "scientific";
        case OUTPUT_FORMAT_FIXED:
            return "fixed";
        case OUTPUT_FORMAT_GENERAL:
        default:
            return "general";
    }
}

static char* skip_spaces(char* text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

static void trim_trailing_spaces(char* text) {
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
}

static int parse_size_value(const char* text, size_t* value) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return 0;
    }

    size_t parsed = 0;
    while (*text != '\0' && !isspace((unsigned char)*text)) {
        if (*text < '0' || *text > '9') {
            return 0;
        }

        const size_t digit = (size_t)(*text - '0');
        if (parsed > (((size_t)-1) - digit) / 10) {
            return 0;
        }
        parsed = parsed * 10 + digit;
        text++;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

static int set_context_decimal_precision(CalculatorMpfrContext* context, size_t digits) {
    const mpfr_prec_t precision = calculator_mpfr_precision_for_decimal_digits(digits);
    CalculatorMpfrResult precision_result;
    calculator_mpfr_result_init(&precision_result, precision);
    const CalculatorStatus status = calculator_mpfr_context_set_precision_checked(
        context,
        precision,
        &precision_result);
    if (status != CALCULATOR_OK) {
        printf("Error: %s\n", precision_result.error);
        calculator_mpfr_result_clear(&precision_result);
        return 0;
    }

    calculator_mpfr_result_clear(&precision_result);
    return 1;
}

static int print_mpfr_special_raw(const mpfr_t value) {
    if (mpfr_nan_p(value)) {
        printf("nan");
        return 1;
    }
    if (mpfr_inf_p(value)) {
        printf("%s", mpfr_sgn(value) < 0 ? "-inf" : "inf");
        return 1;
    }

    return 0;
}

static void print_mpfr_exponent(mpfr_exp_t exponent) {
    printf("e%+ld", (long)exponent);
}

static void print_mpfr_value_fixed_raw(const mpfr_t value, size_t digits_after_point) {
    CalculatorMpfrResult result;
    calculator_mpfr_result_init(&result, mpfr_get_prec(value));
    mpfr_set(result.value, value, MPFR_RNDN);

    char* formatted = NULL;
    const CalculatorStatus status =
        calculator_mpfr_result_format_fixed(&result, digits_after_point, &formatted);
    if (status == CALCULATOR_OK) {
        printf("%s", formatted);
        calculator_mpfr_free_string(formatted);
    } else {
        printf("<format error>");
    }

    calculator_mpfr_result_clear(&result);
}

static void print_mpfr_value_scientific_raw(const mpfr_t value, size_t significant_digits) {
    if (print_mpfr_special_raw(value)) {
        return;
    }

    if (significant_digits == 0) {
        significant_digits = 1;
    }

    if (mpfr_zero_p(value)) {
        printf("0");
        if (significant_digits > 1) {
            printf(".");
            for (size_t i = 1; i < significant_digits; i++) {
                printf("0");
            }
        }
        print_mpfr_exponent(0);
        return;
    }

    mpfr_exp_t exponent = 0;
    char* raw_digits = mpfr_get_str(NULL, &exponent, 10, significant_digits, value, MPFR_RNDN);
    if (raw_digits == NULL) {
        printf("<format error>");
        return;
    }

    const int negative = raw_digits[0] == '-';
    const char* digits = raw_digits + (negative ? 1 : 0);
    size_t digits_length = strlen(digits);

    if (negative) {
        printf("-");
    }
    printf("%c", digits[0]);
    if (significant_digits > 1) {
        printf(".");
        for (size_t i = 1; i < significant_digits; i++) {
            printf("%c", i < digits_length ? digits[i] : '0');
        }
    }
    print_mpfr_exponent(exponent - 1);
    mpfr_free_str(raw_digits);
}

static void print_mpfr_value_general_raw(const mpfr_t value, size_t significant_digits) {
    if (print_mpfr_special_raw(value)) {
        return;
    }

    if (significant_digits == 0) {
        significant_digits = 1;
    }
    if (mpfr_zero_p(value)) {
        printf("0");
        return;
    }

    mpfr_exp_t exponent = 0;
    char* raw_digits = mpfr_get_str(NULL, &exponent, 10, significant_digits, value, MPFR_RNDN);
    if (raw_digits == NULL) {
        printf("<format error>");
        return;
    }

    const int negative = raw_digits[0] == '-';
    const char* digits = raw_digits + (negative ? 1 : 0);
    size_t digits_length = strlen(digits);
    while (digits_length > 1 && digits[digits_length - 1] == '0') {
        digits_length--;
    }

    if (negative) {
        printf("-");
    }

    if (exponent <= -4 || exponent > (mpfr_exp_t)significant_digits) {
        printf("%c", digits[0]);
        if (digits_length > 1) {
            printf(".");
            for (size_t i = 1; i < digits_length; i++) {
                printf("%c", digits[i]);
            }
        }
        print_mpfr_exponent(exponent - 1);
    } else if (exponent <= 0) {
        printf("0.");
        for (mpfr_exp_t i = exponent; i < 0; i++) {
            printf("0");
        }
        for (size_t i = 0; i < digits_length; i++) {
            printf("%c", digits[i]);
        }
    } else if ((size_t)exponent >= digits_length) {
        for (size_t i = 0; i < digits_length; i++) {
            printf("%c", digits[i]);
        }
        for (size_t i = digits_length; i < (size_t)exponent; i++) {
            printf("0");
        }
    } else {
        const size_t integer_digits = (size_t)exponent;
        for (size_t i = 0; i < integer_digits; i++) {
            printf("%c", digits[i]);
        }
        printf(".");
        for (size_t i = integer_digits; i < digits_length; i++) {
            printf("%c", digits[i]);
        }
    }

    mpfr_free_str(raw_digits);
}

static void print_mpfr_value_raw(const mpfr_t value,
                                 OutputFormat override_format,
                                 size_t override_digits) {
    switch (override_format) {
        case OUTPUT_FORMAT_FIXED:
            print_mpfr_value_fixed_raw(value, override_digits);
            break;
        case OUTPUT_FORMAT_SCIENTIFIC:
            print_mpfr_value_scientific_raw(value, override_digits);
            break;
        case OUTPUT_FORMAT_GENERAL:
        default:
            print_mpfr_value_general_raw(value, override_digits);
            break;
    }
}

static void print_mpfr_value(const CalculatorMpfrResult* result,
                             OutputFormat override_format,
                             size_t override_digits) {
    print_mpfr_value_raw(result->value, override_format, override_digits);
}

static void print_result(const CalculatorMpfrResult* result,
                         OutputFormat output_format,
                         size_t output_digits) {
    if (result->status != CALCULATOR_OK) {
        printf("Error: %s\n", result->error);
        return;
    }

    switch (result->kind) {
        case CALCULATOR_RESULT_CONST_SET:
            printf("Set const: %s = ", result->name);
            print_mpfr_value(result, output_format, output_digits);
            printf("\n");
            break;
        case CALCULATOR_RESULT_CONST_DELETED:
            printf("Deleted const: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_SET:
            printf("Set var: %s = ", result->name);
            print_mpfr_value(result, output_format, output_digits);
            printf("\n");
            break;
        case CALCULATOR_RESULT_VAR_DELETED:
            printf("Deleted var: %s\n", result->name);
            break;
        case CALCULATOR_RESULT_VAR_UPDATED:
            printf("Result: ");
            print_mpfr_value(result, output_format, output_digits);
            printf("\n");
            printf("Updated var: %s = ", result->name);
            print_mpfr_value(result, output_format, output_digits);
            printf("\n");
            break;
        case CALCULATOR_RESULT_VALUE:
        default:
            printf("Result: ");
            print_mpfr_value(result, output_format, output_digits);
            printf("\n");
            break;
    }
}

static char* read_dynamic_line(FILE* input) {
    size_t capacity = 128;
    size_t size = 0;
    char* buffer = malloc(capacity);

    if (buffer == NULL) {
        return NULL;
    }

    while (1) {
        if (capacity - size <= 1) {
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

        const size_t available = capacity - size;
        const int read_size = available > (size_t)INT_MAX ? INT_MAX : (int)available;

        if (fgets(buffer + size, read_size, input) == NULL) {
            break;
        }
        size += strlen(buffer + size);
        if (size > 0 && buffer[size - 1] == '\n') {
            buffer[size - 1] = '\0';
            return buffer;
        }
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

static void strip_line_comment(char* line) {
    char* comment = strstr(line, "//");
    if (comment != NULL) {
        *comment = '\0';
        trim_trailing_spaces(line);
    }
}

static void print_symbols(const CalculatorShell* shell,
                          const CalculatorMpfrContext* context) {
    if (context->count == 0) {
        printf("No session symbols\n");
        return;
    }

    for (size_t i = 0; i < context->count; i++) {
        const CalculatorMpfrSymbol* symbol = &context->symbols[i];
        printf("%s %s = ",
               symbol->kind == CALCULATOR_SYMBOL_CONST ? "const" : "var",
               symbol->name);
        print_mpfr_value_raw(symbol->value, shell->format,
                             shell->format == OUTPUT_FORMAT_FIXED
                                 ? shell->fixed_digits
                                 : shell->precision_digits);
        if (symbol->source_expression != NULL) {
            printf("  [source: %s]", symbol->source_expression);
        }
        printf("\n");
    }
}

static ProcessLineResult handle_shell_command(CalculatorShell* shell,
                                              CalculatorMpfrContext* context,
                                              char* command) {
    char* cursor = skip_spaces(command + 1);
    trim_trailing_spaces(cursor);

    if (strcmp(cursor, "vars") == 0) {
        print_symbols(shell, context);
        return PROCESS_OK;
    }

    if (strcmp(cursor, "clear") == 0) {
        calculator_mpfr_context_clear_symbols(context);
        printf("Cleared session symbols\n");
        return PROCESS_OK;
    }

    if (strncmp(cursor, "precision", 9) == 0 &&
        (cursor[9] == '\0' || isspace((unsigned char)cursor[9]))) {
        size_t digits = 0;
        if (!parse_size_value(cursor + 9, &digits)) {
            printf("Error: expected precision digits\n");
            return PROCESS_ERROR;
        }
        if (!set_context_decimal_precision(context, digits)) {
            return PROCESS_ERROR;
        }

        shell->precision_digits = digits;
        printf("Precision: %zu\n", shell->precision_digits);
        return PROCESS_OK;
    }

    if (strncmp(cursor, "format", 6) == 0 &&
        (cursor[6] == '\0' || isspace((unsigned char)cursor[6]))) {
        char* format = skip_spaces(cursor + 6);
        if (strcmp(format, "") == 0) {
            if (shell->format == OUTPUT_FORMAT_FIXED) {
                printf("Format: fixed %zu\n", shell->fixed_digits);
            } else {
                printf("Format: %s\n", format_name(shell->format));
            }
            return PROCESS_OK;
        }
        if (strcmp(format, "general") == 0) {
            shell->format = OUTPUT_FORMAT_GENERAL;
            printf("Format: general\n");
            return PROCESS_OK;
        }
        if (strcmp(format, "scientific") == 0) {
            shell->format = OUTPUT_FORMAT_SCIENTIFIC;
            printf("Format: scientific\n");
            return PROCESS_OK;
        }
        if (strncmp(format, "fixed", 5) == 0 &&
            (format[5] == '\0' || isspace((unsigned char)format[5]))) {
            size_t digits = 0;
            if (!parse_size_value(format + 5, &digits)) {
                printf("Error: expected fixed digits\n");
                return PROCESS_ERROR;
            }
            shell->format = OUTPUT_FORMAT_FIXED;
            shell->fixed_digits = digits;
            printf("Format: fixed %zu\n", shell->fixed_digits);
            return PROCESS_OK;
        }

        printf("Error: unknown format\n");
        return PROCESS_ERROR;
    }

    printf("Error: unknown command\n");
    return PROCESS_ERROR;
}

static int update_ans(CalculatorMpfrContext* context, const CalculatorMpfrResult* result) {
    if (result->status != CALCULATOR_OK ||
        result->kind == CALCULATOR_RESULT_CONST_DELETED ||
        result->kind == CALCULATOR_RESULT_VAR_DELETED) {
        return 1;
    }

    CalculatorMpfrResult ans_result;
    calculator_mpfr_result_init(&ans_result, context->precision);
    const CalculatorStatus status = calculator_mpfr_context_set_ans(context, result->value, &ans_result);
    if (status != CALCULATOR_OK) {
        printf("Error: cannot update ans: %s\n", ans_result.error);
        calculator_mpfr_result_clear(&ans_result);
        return 0;
    }

    calculator_mpfr_result_clear(&ans_result);
    return 1;
}

static ProcessLineResult process_line(CalculatorShell* shell,
                                      CalculatorMpfrContext* context,
                                      char* line) {
    strip_line_comment(line);
    char* expression = skip_spaces(line);
    trim_trailing_spaces(expression);

    if (strcmp(expression, "exit") == 0) {
        return PROCESS_EXIT;
    }

    if (strcmp(expression, "help") == 0) {
        print_help();
        return PROCESS_OK;
    }

    if (expression[0] == '\0') {
        return PROCESS_OK;
    }

    if (expression[0] == ':') {
        return handle_shell_command(shell, context, expression);
    }

    size_t output_digits = shell->format == OUTPUT_FORMAT_FIXED
                               ? shell->fixed_digits
                               : shell->precision_digits;
    OutputFormat output_format = shell->format;
    size_t evaluation_digits = shell->precision_digits;

    const int has_precision_suffix = parse_precision_suffix(expression, &output_digits);
    if (has_precision_suffix < 0) {
        printf("Error: invalid precision suffix\n");
        return PROCESS_ERROR;
    }
    if (has_precision_suffix > 0) {
        output_format = OUTPUT_FORMAT_FIXED;
        evaluation_digits = output_digits;
    }

    if (!set_context_decimal_precision(context, evaluation_digits)) {
        return PROCESS_ERROR;
    }

    CalculatorMpfrResult result;
    calculator_mpfr_result_init(&result, context->precision);
    calculator_mpfr_context_evaluate(context, expression, &result);
    print_result(&result, output_format, output_digits);

    const int ans_updated = update_ans(context, &result);
    const int ok = result.status == CALCULATOR_OK && ans_updated;
    calculator_mpfr_result_clear(&result);
    return ok ? PROCESS_OK : PROCESS_ERROR;
}

static int parse_format_option(CalculatorShell* shell, int argc, char** argv, int* index) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "Error: --format requires a value\n");
        return 0;
    }

    const char* value = argv[++(*index)];
    if (strcmp(value, "general") == 0) {
        shell->format = OUTPUT_FORMAT_GENERAL;
        return 1;
    }
    if (strcmp(value, "scientific") == 0) {
        shell->format = OUTPUT_FORMAT_SCIENTIFIC;
        return 1;
    }
    if (strcmp(value, "fixed") == 0) {
        if (*index + 1 >= argc || !parse_size_value(argv[*index + 1], &shell->fixed_digits)) {
            fprintf(stderr, "Error: --format fixed requires digits\n");
            return 0;
        }
        (*index)++;
        shell->format = OUTPUT_FORMAT_FIXED;
        return 1;
    }

    fprintf(stderr, "Error: unknown format: %s\n", value);
    return 0;
}

static int run_expression(CalculatorShell* shell,
                          CalculatorMpfrContext* context,
                          const char* expression) {
    char* line = malloc(strlen(expression) + 1);
    if (line == NULL) {
        fprintf(stderr, "Error: cannot allocate expression buffer\n");
        return 1;
    }
    strcpy(line, expression);

    const ProcessLineResult result = process_line(shell, context, line);
    free(line);
    return result == PROCESS_OK ? 0 : 1;
}

static int run_file(CalculatorShell* shell,
                    CalculatorMpfrContext* context,
                    const char* path) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open file: %s\n", path);
        return 1;
    }

    int status = 0;
    char* line = NULL;
    while ((line = read_dynamic_line(file)) != NULL) {
        const ProcessLineResult result = process_line(shell, context, line);
        free(line);
        if (result == PROCESS_EXIT) {
            break;
        }
        if (result == PROCESS_ERROR) {
            status = 1;
        }
    }

    fclose(file);
    return status;
}

int main(int argc, char** argv) {
    CalculatorShell shell;
    shell.precision_digits = DEFAULT_DECIMAL_DIGITS;
    shell.format = OUTPUT_FORMAT_GENERAL;
    shell.fixed_digits = DEFAULT_DECIMAL_DIGITS;

    const char* expression = NULL;
    const char* file_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -e requires an expression\n");
                return 1;
            }
            expression = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --file requires a path\n");
                return 1;
            }
            file_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--precision") == 0) {
            if (i + 1 >= argc || !parse_size_value(argv[i + 1], &shell.precision_digits)) {
                fprintf(stderr, "Error: --precision requires digits\n");
                return 1;
            }
            i++;
            continue;
        }
        if (strcmp(argv[i], "--format") == 0) {
            if (!parse_format_option(&shell, argc, argv, &i)) {
                return 1;
            }
            continue;
        }

        fprintf(stderr, "Error: unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    if (expression != NULL && file_path != NULL) {
        fprintf(stderr, "Error: use either -e or --file, not both\n");
        return 1;
    }

    CalculatorMpfrContext context;
    calculator_mpfr_context_init(
        &context,
        calculator_mpfr_precision_for_decimal_digits(shell.precision_digits),
        MPFR_RNDN);

    int exit_code = 0;
    if (expression != NULL) {
        exit_code = run_expression(&shell, &context, expression);
    } else if (file_path != NULL) {
        exit_code = run_file(&shell, &context, file_path);
    } else {
        printf("Type 'help' for available commands and functions\n");
        printf("Type 'exit' to quit\n\n");

        while (1) {
            printf(">> ");

            char* line = read_dynamic_line(stdin);
            if (line == NULL) {
                if (!feof(stdin)) {
                    printf("Input error\n");
                    exit_code = 1;
                }
                break;
            }

            const ProcessLineResult result = process_line(&shell, &context, line);
            free(line);
            if (result == PROCESS_EXIT) {
                break;
            }
        }

        printf("Goodbye!\n");
    }

    calculator_mpfr_context_free(&context);
    return exit_code;
}
