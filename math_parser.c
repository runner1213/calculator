#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_TOKENS 100
#define MAX_STACK 100
#define MAX_FUNC_LEN 10

typedef enum {
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_FUNCTION,
    TOKEN_COMMA,
    TOKEN_END
} TokenType;

typedef struct {
    TokenType type;
    union {
        double num;
        char op;
        char func[MAX_FUNC_LEN];
    } value;
    int precedence;
    int is_right_assoc;
    int argc;
} Token;

typedef struct {
    double data[MAX_STACK];
    int top;
} Stack;

static void stack_init(Stack* s) {
    s->top = -1;
}

static int stack_push(Stack* s, double value) {
    if (s->top >= MAX_STACK - 1) return 0;
    s->data[++(s->top)] = value;
    return 1;
}

static double stack_pop(Stack* s) {
    if (s->top < 0) return 0;
    return s->data[(s->top)--];
}

static double stack_peek(Stack* s) {
    if (s->top < 0) return 0;
    return s->data[s->top];
}

static int is_stack_empty(Stack* s) {
    return s->top == -1;
}

static int get_precedence(char op) {
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': return 2;
        case '^': return 3;
        default: return 0;
    }
}

static int is_right_associative(char op) {
    return op == '^';
}

static int is_function_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static double apply_function(const char* func, double arg) {
    if (strcmp(func, "sqrt") == 0) {
        if (arg < 0) {
            printf("Error: Square root of negative number\n");
            return NAN;
        }
        return sqrt(arg);
    }
    else if (strcmp(func, "sin") == 0) {
        return sin(arg);
    }
    else if (strcmp(func, "cos") == 0) {
        return cos(arg);
    }
    else if (strcmp(func, "tan") == 0) {
        return tan(arg);
    }
    else if (strcmp(func, "log") == 0) {
        if (arg <= 0) {
            printf("Error: Logarithm of non-positive number\n");
            return NAN;
        }
        return log10(arg);
    }
    else if (strcmp(func, "ln") == 0) {
        if (arg <= 0) {
            printf("Error: Natural log of non-positive number\n");
            return NAN;
        }
        return log(arg);
    }
    else if (strcmp(func, "abs") == 0) {
        return fabs(arg);
    }
    else if (strcmp(func, "exp") == 0) {
        return exp(arg);
    }

    printf("Error: Unknown function '%s'\n", func);
    return NAN;
}

static int tokenize(const char* expr, Token* tokens) {
    const char* ptr = expr;
    int count = 0;
    int expecting_operand = 1;

    while (*ptr != '\0' && count < MAX_TOKENS) {
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;

        if (isdigit(*ptr) || *ptr == '.') {
            char* end;
            tokens[count].type = TOKEN_NUMBER;
            tokens[count].value.num = strtod(ptr, &end);
            ptr = end;
            expecting_operand = 0;
            count++;
        }
        else if (is_function_char(*ptr)) {
            tokens[count].type = TOKEN_FUNCTION;
            int i = 0;
            while (is_function_char(*ptr) && i < MAX_FUNC_LEN - 1) {
                tokens[count].value.func[i++] = *ptr++;
            }
            tokens[count].value.func[i] = '\0';
            tokens[count].argc = 1;
            expecting_operand = 1;
            count++;
        }
        else if (*ptr == '(') {
            tokens[count].type = TOKEN_LPAREN;
            tokens[count].value.op = *ptr;
            ptr++;
            expecting_operand = 1;
            count++;
        }
        else if (*ptr == ')') {
            tokens[count].type = TOKEN_RPAREN;
            tokens[count].value.op = *ptr;
            ptr++;
            expecting_operand = 0;
            count++;
        }
        else if (*ptr == ',') {
            tokens[count].type = TOKEN_COMMA;
            tokens[count].value.op = *ptr;
            ptr++;
            expecting_operand = 1;
            count++;
        }
        else if (*ptr == '+' || *ptr == '-' || *ptr == '*' || *ptr == '/' || *ptr == '^') {
            if (expecting_operand && (*ptr == '+' || *ptr == '-')) {
                tokens[count].type = TOKEN_NUMBER;
                tokens[count].value.num = (*ptr == '-') ? -1 : 1;
                count++;

                tokens[count].type = TOKEN_OPERATOR;
                tokens[count].value.op = '*';
                tokens[count].precedence = get_precedence('*');
                tokens[count].is_right_assoc = 0;
                ptr++;
                count++;
            } else {
                tokens[count].type = TOKEN_OPERATOR;
                tokens[count].value.op = *ptr;
                tokens[count].precedence = get_precedence(*ptr);
                tokens[count].is_right_assoc = is_right_associative(*ptr);
                ptr++;
                expecting_operand = 1;
                count++;
            }
        }
        else {
            printf("Error: Unknown character '%c'\n", *ptr);
            return -1;
        }
    }

    tokens[count].type = TOKEN_END;
    return count;
}

static void infix_to_rpn(Token* tokens, Token* rpn, int token_count) {
    Token op_stack[MAX_TOKENS];
    int op_top = -1;
    int rpn_index = 0;

    for (int i = 0; i < token_count; i++) {
        Token t = tokens[i];

        if (t.type == TOKEN_NUMBER) {
            rpn[rpn_index++] = t;
        }
        else if (t.type == TOKEN_FUNCTION) {
            op_stack[++op_top] = t;
        }
        else if (t.type == TOKEN_OPERATOR) {
            while (op_top >= 0 && op_stack[op_top].type == TOKEN_OPERATOR &&
                   ((!t.is_right_assoc && t.precedence <= op_stack[op_top].precedence) ||
                    (t.is_right_assoc && t.precedence < op_stack[op_top].precedence))) {
                rpn[rpn_index++] = op_stack[op_top--];
            }
            op_stack[++op_top] = t;
        }
        else if (t.type == TOKEN_LPAREN) {
            op_stack[++op_top] = t;
        }
        else if (t.type == TOKEN_RPAREN) {
            while (op_top >= 0 && op_stack[op_top].type != TOKEN_LPAREN) {
                rpn[rpn_index++] = op_stack[op_top--];
            }
            if (op_top >= 0 && op_stack[op_top].type == TOKEN_LPAREN) {
                op_top--;
            }
            if (op_top >= 0 && op_stack[op_top].type == TOKEN_FUNCTION) {
                rpn[rpn_index++] = op_stack[op_top--];
            }
        }
        else if (t.type == TOKEN_COMMA) {
            while (op_top >= 0 && op_stack[op_top].type != TOKEN_LPAREN) {
                rpn[rpn_index++] = op_stack[op_top--];
            }
        }
    }

    while (op_top >= 0) {
        rpn[rpn_index++] = op_stack[op_top--];
    }

    rpn[rpn_index].type = TOKEN_END;
}

static double evaluate_rpn(Token* rpn) {
    Stack stack;
    stack_init(&stack);

    for (int i = 0; rpn[i].type != TOKEN_END; i++) {
        Token t = rpn[i];

        if (t.type == TOKEN_NUMBER) {
            stack_push(&stack, t.value.num);
        }
        else if (t.type == TOKEN_OPERATOR) {
            double b = stack_pop(&stack);
            double a = stack_pop(&stack);
            double result;

            switch (t.value.op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/':
                    if (b == 0) {
                        printf("Error: Division by zero\n");
                        return NAN;
                    }
                    result = a / b;
                    break;
                case '^': result = pow(a, b); break;
                default: return NAN;
            }

            stack_push(&stack, result);
        }
        else if (t.type == TOKEN_FUNCTION) {
            double arg = stack_pop(&stack);
            double result = apply_function(t.value.func, arg);
            if (isnan(result)) return NAN;
            stack_push(&stack, result);
        }
    }

    return stack_pop(&stack);
}

double parser(const char expression[]) {
    Token tokens[MAX_TOKENS];
    Token rpn[MAX_TOKENS];

    int token_count = tokenize(expression, tokens);
    if (token_count < 0) {
        return NAN;
    }

    infix_to_rpn(tokens, rpn, token_count);

    return evaluate_rpn(rpn);
}
