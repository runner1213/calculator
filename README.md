## === MATH CALCULATOR HELP ===
### Supported operations:
- Basic: +  -  *  /
- Power: ^ (example: 2^3 = 8)
- Factorial: ! (example: 5! = 120)
- Parentheses: ( ) for grouping

### Mathematical functions:
- `sqrt(x)`   - square root
- `sin(x)`    - sine (radians)
- `cos(x)`    - cosine (radians)
- `tan(x)`    - tangent (radians)
- `log(x)`    - base-10 logarithm
- `ln(x)`     - natural logarithm
- `abs(x)`    - absolute value
- `exp(x)`    - exponential
- `deg(x)`    - convert degrees to radians

### Constants:
- `pi`        - 3.141592653589793...
- `c`         - speed of light in vacuum, m/s
- `G`         - gravitational constant, N*m^2/kg^2
- `h`         - Planck constant, J*s
- `k`         - Boltzmann constant, J/K

### Session symbols:
- `const name = expression`  - set a constant value
- `const name = null `       - delete a constant value
- `var name = expression`    - set a mutable accumulator value
- `var name = null`          - delete a mutable accumulator value
- `name = expression`        - shorthand for const name = expression

## Examples:
- `sqrt(25)`             = 5
- `2 + 3 * 4`            = 14
- `(2 + 3) * 4`          = 20
- `(2 + 3)!`             = 120
- `2^3 + 1`              = 9
- `sin(0) + cos(0)`      = 1
- `sin(deg(90))`         = 1
- `c^2`                  = 8.987551787e16
- `cos(pi)`              = -1
- `sqrt(2^4 + 3^2)`      = 5
- `log(100)`             = 2
- `const rate = 6.09`    - save session constant
- `369 / rate`           = 60.5911330049261
- `var total = 10`       - save session accumulator
- `total + 5`            = 15, then total becomes 15

### Special commands:
- `help` - show this help
- `exit` - quit program

### MPFR precision:
- The CLI uses the MPFR backend.
- Add `:digits` to the end of an input line to print a fixed number of digits after the decimal point.
- The suffix is CLI-only and is not part of the expression grammar.
- Examples:
  - `pi:80`
  - `1 / 3:50`
  - `const p = pi:100`

### C API:
- The old `double` API remains available:
  - `calculator_evaluate(expression)`
  - `calculator_context_evaluate(context, expression)`
  - `parser(expression)`
- MPFR API is available through `include/calculator/calculator_mpfr.h`.
- Result values are stored in `mpfr_t`, so callers initialize and clear `CalculatorMpfrResult`.
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

============================
