#include "calculator/calculator.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

typedef struct {
    int is_assignment;
    CalculatorSymbolKind kind;
    char name[CALCULATOR_NAME_SIZE];
    const char* expression;
} Assignment;

static void init_result(CalculatorResult* result) {
    result->status = CALCULATOR_OK;
    result->kind = CALCULATOR_RESULT_VALUE;
    result->value = NAN;
    result->name[0] = '\0';
    result->error[0] = '\0';
}

static void set_error(CalculatorResult* result, CalculatorStatus status, const char* message) {
    if (result->status != CALCULATOR_OK) {
        return;
    }

    result->status = status;
    strncpy(result->error, message, CALCULATOR_ERROR_SIZE - 1);
    result->error[CALCULATOR_ERROR_SIZE - 1] = '\0';
    result->value = NAN;
}

static void set_result_name(CalculatorResult* result, const char* name) {
    strncpy(result->name, name, CALCULATOR_NAME_SIZE - 1);
    result->name[CALCULATOR_NAME_SIZE - 1] = '\0';
}

static char* duplicate_string(const char* value) {
    const size_t length = strlen(value);
    char* copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1);
    return copy;
}

void calculator_context_init(CalculatorContext* context) {
    if (context == NULL) {
        return;
    }

    context->symbols = NULL;
    context->count = 0;
    context->capacity = 0;
}

void calculator_context_free(CalculatorContext* context) {
    if (context == NULL) {
        return;
    }

    for (size_t i = 0; i < context->count; i++) {
        free(context->symbols[i].name);
    }

    free(context->symbols);
    context->symbols = NULL;
    context->count = 0;
    context->capacity = 0;
}

static int find_symbol_index(const CalculatorContext* context, const char* name, size_t* index) {
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

static int is_builtin_or_keyword(const char* name) {
    static const char* reserved[] = {
        "abs", "const", "cos", "deg", "degrees", "exp", "ln", "log",
        "null", "sin", "sqrt", "tan", "var",
        "G", "c", "h", "k", "pi"
    };

    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcmp(name, reserved[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static int ensure_capacity(CalculatorContext* context, CalculatorResult* result) {
    if (context->count < context->capacity) {
        return 1;
    }

    const size_t new_capacity = context->capacity == 0 ? 8 : context->capacity * 2;
    if (new_capacity < context->capacity) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Too many session symbols");
        return 0;
    }

    CalculatorSymbol* symbols = realloc(context->symbols, new_capacity * sizeof(*symbols));
    if (symbols == NULL) {
        set_error(result, CALCULATOR_ERROR_MEMORY, "Cannot allocate session symbol table");
        return 0;
    }

    context->symbols = symbols;
    context->capacity = new_capacity;
    return 1;
}

static void set_kind_result(CalculatorResult* result, CalculatorSymbolKind kind, int deleted) {
    if (kind == CALCULATOR_SYMBOL_CONST) {
        result->kind = deleted ? CALCULATOR_RESULT_CONST_DELETED : CALCULATOR_RESULT_CONST_SET;
    } else {
        result->kind = deleted ? CALCULATOR_RESULT_VAR_DELETED : CALCULATOR_RESULT_VAR_SET;
    }
}

static CalculatorResult set_symbol(CalculatorContext* context,
                                   const char* name,
                                   double value,
                                   CalculatorSymbolKind kind) {
    CalculatorResult result;
    init_result(&result);

    if (is_builtin_or_keyword(name)) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Cannot use reserved name");
        return result;
    }

    size_t index = 0;
    if (find_symbol_index(context, name, &index)) {
        CalculatorSymbol* symbol = &context->symbols[index];
        if (symbol->kind != kind) {
            set_error(&result, CALCULATOR_ERROR_SYNTAX, "Name already exists with another symbol kind");
            return result;
        }

        symbol->value = value;
        result.value = value;
        set_result_name(&result, name);
        set_kind_result(&result, kind, 0);
        return result;
    }

    if (!ensure_capacity(context, &result)) {
        return result;
    }

    char* name_copy = duplicate_string(name);
    if (name_copy == NULL) {
        set_error(&result, CALCULATOR_ERROR_MEMORY, "Cannot allocate symbol name");
        return result;
    }

    context->symbols[context->count].name = name_copy;
    context->symbols[context->count].value = value;
    context->symbols[context->count].kind = kind;
    context->count++;

    result.value = value;
    set_result_name(&result, name);
    set_kind_result(&result, kind, 0);
    return result;
}

static CalculatorResult delete_symbol(CalculatorContext* context,
                                      const char* name,
                                      CalculatorSymbolKind kind) {
    CalculatorResult result;
    init_result(&result);

    size_t index = 0;
    if (!find_symbol_index(context, name, &index)) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Unknown symbol");
        return result;
    }

    if (context->symbols[index].kind != kind) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Symbol has another kind");
        return result;
    }

    free(context->symbols[index].name);
    if (index + 1 < context->count) {
        memmove(&context->symbols[index],
                &context->symbols[index + 1],
                (context->count - index - 1) * sizeof(context->symbols[0]));
    }
    context->count--;

    set_result_name(&result, name);
    set_kind_result(&result, kind, 1);
    return result;
}

static CalculatorResult evaluate_expression(CalculatorContext* context,
                                            const char* expression,
                                            int auto_update_var) {
    CalculatorResult result;
    init_result(&result);

    if (expression == NULL) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Expression is NULL");
        return result;
    }

    Parser parser;
    parser.result = &result;
    parser.context = context;
    parser.depth = 0;
    parser.auto_update_var = auto_update_var;
    parser.has_mutable_reference = 0;
    parser.has_multiple_mutable_references = 0;
    parser.mutable_reference_index = 0;
    lexer_init(&parser.lexer, expression, &result);

    if (result.status == CALCULATOR_OK) {
        result.value = parse_expression(&parser);
    }
    if (result.status == CALCULATOR_OK && parser.lexer.current.type != TOKEN_END) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Unexpected trailing input");
    }
    if (result.status == CALCULATOR_OK &&
        auto_update_var &&
        context != NULL &&
        parser.has_mutable_reference &&
        !parser.has_multiple_mutable_references) {
        CalculatorSymbol* symbol = &context->symbols[parser.mutable_reference_index];
        symbol->value = result.value;
        result.kind = CALCULATOR_RESULT_VAR_UPDATED;
        set_result_name(&result, symbol->name);
    }
    if (result.status != CALCULATOR_OK) {
        result.value = NAN;
    }

    return result;
}

static int parse_assignment_header(const char* input, Assignment* assignment, CalculatorResult* result) {
    assignment->is_assignment = 0;
    assignment->kind = CALCULATOR_SYMBOL_CONST;
    assignment->name[0] = '\0';
    assignment->expression = NULL;

    Lexer lexer;
    lexer_init(&lexer, input, result);
    if (result->status != CALCULATOR_OK || lexer.current.type != TOKEN_IDENTIFIER) {
        return 0;
    }

    char first[CALCULATOR_NAME_SIZE];
    strncpy(first, lexer.current.text, CALCULATOR_NAME_SIZE - 1);
    first[CALCULATOR_NAME_SIZE - 1] = '\0';
    lexer_next(&lexer);
    if (result->status != CALCULATOR_OK) {
        return 0;
    }

    if (strcmp(first, "const") == 0 || strcmp(first, "var") == 0) {
        assignment->is_assignment = 1;
        assignment->kind = strcmp(first, "var") == 0 ? CALCULATOR_SYMBOL_VAR : CALCULATOR_SYMBOL_CONST;

        if (lexer.current.type != TOKEN_IDENTIFIER) {
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Expected symbol name after modifier");
            return 1;
        }

        strncpy(assignment->name, lexer.current.text, CALCULATOR_NAME_SIZE - 1);
        assignment->name[CALCULATOR_NAME_SIZE - 1] = '\0';
        lexer_next(&lexer);
        if (result->status != CALCULATOR_OK) {
            return 1;
        }

        if (lexer.current.type != TOKEN_EQUAL) {
            set_error(result, CALCULATOR_ERROR_SYNTAX, "Expected '=' after symbol name");
            return 1;
        }

        lexer_next(&lexer);
        assignment->expression = input + lexer.current.position;
        return 1;
    }

    if (lexer.current.type != TOKEN_EQUAL) {
        return 0;
    }

    assignment->is_assignment = 1;
    assignment->kind = CALCULATOR_SYMBOL_CONST;
    strncpy(assignment->name, first, CALCULATOR_NAME_SIZE - 1);
    assignment->name[CALCULATOR_NAME_SIZE - 1] = '\0';

    lexer_next(&lexer);
    assignment->expression = input + lexer.current.position;
    return 1;
}

static int is_null_literal(const char* expression) {
    CalculatorResult result;
    init_result(&result);

    Lexer lexer;
    lexer_init(&lexer, expression, &result);
    if (result.status != CALCULATOR_OK || lexer.current.type != TOKEN_IDENTIFIER ||
        strcmp(lexer.current.text, "null") != 0) {
        return 0;
    }

    lexer_next(&lexer);
    return result.status == CALCULATOR_OK && lexer.current.type == TOKEN_END;
}

CalculatorResult calculator_evaluate(const char* expression) {
    return evaluate_expression(NULL, expression, 0);
}

CalculatorResult calculator_context_evaluate(CalculatorContext* context, const char* expression) {
    CalculatorResult result;
    init_result(&result);

    if (context == NULL) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Calculator context is NULL");
        return result;
    }
    if (expression == NULL) {
        set_error(&result, CALCULATOR_ERROR_SYNTAX, "Expression is NULL");
        return result;
    }

    Assignment assignment;
    if (!parse_assignment_header(expression, &assignment, &result)) {
        if (result.status != CALCULATOR_OK) {
            return result;
        }
        return evaluate_expression(context, expression, 1);
    }
    if (result.status != CALCULATOR_OK) {
        return result;
    }

    if (is_null_literal(assignment.expression)) {
        return delete_symbol(context, assignment.name, assignment.kind);
    }

    CalculatorResult value_result = evaluate_expression(context, assignment.expression, 0);
    if (value_result.status != CALCULATOR_OK) {
        return value_result;
    }

    return set_symbol(context, assignment.name, value_result.value, assignment.kind);
}

double parser(const char expression[]) {
    const CalculatorResult result = calculator_evaluate(expression);
    return result.status == CALCULATOR_OK ? result.value : NAN;
}
