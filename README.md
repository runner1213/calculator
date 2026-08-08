## Math Calculator

### Supported operations
- Basic: `+` `-` `*` `/`
- Power: `^` (example: `2^3 = 8`)
- Factorial: `!` (example: `5! = 120`)
- Parentheses: `( )` for grouping
- Complex numbers: `i` (examples: `sqrt(-1)`, `2 + 3i`, `(1+i)^2`)

Factorial is supported for real non-negative integers.

### Mathematical functions
- `sqrt(x)` - square root
- `sin(x)` - sine in radians
- `cos(x)` - cosine in radians
- `tan(x)` - tangent in radians
- `log(x)` - base-10 logarithm
- `ln(x)` - natural logarithm
- `abs(x)` - absolute value
- `exp(x)` - exponential
- `deg(x)` or `degrees(x)` - convert degrees to radians

### Built-in constants
- `pi` - 3.141592653589793...
- `i` - imaginary unit
- `ans` - previous successful result in the current session
- `c` - speed of light in vacuum, m/s
- `G` - gravitational constant, N*m^2/kg^2
- `h` - Planck constant, J*s
- `k` - Boltzmann constant, J/K

### Session symbols
- `const name = expression` - set a recalculated session constant.
- `const name = null` - delete a constant.
- `var name = expression` - set a mutable accumulator value.
- `var name = null` - delete a mutable accumulator.
- `name = expression` - shorthand for `const name = expression`.

Constants store the original right-hand expression and are recalculated when the MPFR precision changes. For example, `const p = pi` followed by `p:100` recalculates `p` at the higher precision before printing.

Variables store the current numeric value. They do not keep a source expression, so their meaningful precision is fixed by the assignment or accumulator update that produced them.

### Interactive commands
- `help` - show help.
- `exit` - quit.
- `:precision digits` - set the default evaluation precision in decimal digits.
- `:format general` - print significant digits.
- `:format scientific` - print scientific notation.
- `:format fixed digits` - print fixed digits after the decimal point.
- `:vars` - list current session symbols and stored constant sources.
- `:clear` - clear all session symbols, including `ans`.

Formatting changes output only. Use `:precision digits`, `--precision digits`, or a per-line `expression:digits` suffix when you need more evaluation precision.

### Precision suffix
- `expression:digits` evaluates and prints one line in fixed format with `digits` digits after the decimal point.
- The suffix is CLI-only and is not part of the expression grammar.
- Examples:
  - `pi:80`
  - `1 / 3:50`
  - `const p = pi`
  - `p:100`

### Command line

Run the interactive shell:

```sh
calculator
```

Evaluate one expression:

```sh
calculator -e "sin(pi/7)" --precision 200
```

Evaluate a file line by line:

```sh
calculator --file calculations.txt
```

Options:
- `--precision digits` sets the default evaluation precision before processing `-e` or `--file`.
- `--format general` uses significant-digit output.
- `--format scientific` uses scientific notation.
- `--format fixed digits` uses fixed decimal output.

Lines in `--file` may contain shell commands such as `:precision 200`, expressions, and `//` comments.

### C API
- The old `double` API remains available:
  - `calculator_evaluate(expression)`
  - `calculator_context_evaluate(context, expression)`
  - `parser(expression)`
- MPFR API is available through `include/calculator/calculator_mpfr.h`.
- Result values store real and imaginary `mpfr_t` parts. Callers initialize and clear `CalculatorMpfrResult`.
- `CalculatorMpfrResult::is_complex` indicates whether `imaginary_value` is part of the result.
- `calculator_mpfr_context_set_precision_checked(context, precision, result)` changes context precision and refreshes stored constants that have source expressions.
- `calculator_mpfr_context_clear_symbols(context)` removes all session symbols.
- `calculator_mpfr_context_set_ans(context, value, result)` updates the internal `ans` symbol.
- Fixed decimal output uses function parameters instead of the CLI `:digits` suffix:

```c
CalculatorMpfrContext context;
calculator_mpfr_context_init(
    &context,
    calculator_mpfr_precision_for_decimal_digits(80),
    MPFR_RNDN);

CalculatorMpfrResult result;
calculator_mpfr_result_init(&result, context.precision);

char* output = NULL;
CalculatorStatus status = calculator_mpfr_context_evaluate_fixed(
    &context,
    "pi",
    80,
    &result,
    &output);

if (status == CALCULATOR_OK) {
    puts(output);
}

calculator_mpfr_free_string(output);
calculator_mpfr_result_clear(&result);
calculator_mpfr_context_free(&context);
```
