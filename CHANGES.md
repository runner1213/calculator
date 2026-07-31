# Changes

## Interface commands and command line
- `src/main.c:18` adds `CalculatorShell`, the shared shell state for default precision, output format, fixed digits, and batch mode.
- `src/main.c:387` adds `handle_shell_command`, which implements `:precision`, `:format general`, `:format scientific`, `:format fixed digits`, `:vars`, and `:clear`.
- `src/main.c:482` adds `process_line`, the single execution path used by REPL, `-e`, and `--file`.
- `src/main.c:537` adds `parse_format_option` for command-line `--format general|scientific|fixed digits`.
- `src/main.c:566` adds `run_expression` for `calculator -e "expression"`.
- `src/main.c:581` adds `run_file` for `calculator --file calculations.txt`.
- `src/main.c:607` changes `main(void)` to `main(int argc, char** argv)` and parses `-e`, `--file`, `--precision`, `--format`, and `--help`.
- `src/main.c:471` updates the reserved `ans` symbol after every successful result that produces a value.
- `src/main.c:399` makes `:clear` remove all session symbols, including `ans`.
- `src/main.c:372` makes `:vars` print all session symbols and show `[source: ...]` for recalculated constants.
- `src/main.c:345` strips `//` line comments before parsing expressions or shell commands. This is mainly for `--file` input.

## Precision bug fix
- `include/calculator/calculator_mpfr.h:16` adds `source_expression` to `CalculatorMpfrSymbol`.
- `src/calculator_mpfr.c:229` stores the right-hand expression only for `const` symbols. `var` symbols keep only their current numeric value.
- `src/calculator_mpfr.c:363` adds `refresh_constants`, which recalculates stored constants from `source_expression` when context precision changes.
- `src/calculator_mpfr.c:94` adds `calculator_mpfr_context_set_precision_checked`, which changes precision and calls `refresh_constants`.
- `src/calculator_mpfr.c:605` updates `calculator_mpfr_context_evaluate_fixed` to use the checked precision path, so API fixed-output calls also refresh constants.
- Decision: `const p = pi` followed by `p:100` now recalculates `p` from `pi` at the higher precision before printing.
- Decision: `var` values are snapshots. Their source expressions are not stored, because `var` also acts as a mutable accumulator and auto-updates from later expressions.
- Decision: `ans` is stored as a reserved internal constant without a source expression. Users can read `ans`, but cannot assign to it directly.

## Public MPFR API
- `include/calculator/calculator_mpfr.h:44` declares `calculator_mpfr_context_set_precision_checked`.
- `include/calculator/calculator_mpfr.h:47` declares `calculator_mpfr_context_clear_symbols`.
- `include/calculator/calculator_mpfr.h:50` declares `calculator_mpfr_context_set_ans`.

## Documentation
- `README.md:35` documents that constants store source expressions and are recalculated on precision changes.
- `README.md:37` documents that variables keep numeric snapshot values.
- `README.md:39` documents the new interactive shell commands.
- `README.md:49` documents that format changes do not increase evaluation precision.
- `README.md:60` documents `calculator -e`, `calculator --file`, `--precision`, and `--format`.
- `README.md:95` documents the new MPFR API helpers.
