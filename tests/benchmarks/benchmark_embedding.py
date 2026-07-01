#!/usr/bin/env python3
"""tests/benchmarks/benchmark_embedding.py — compare Tier 1 vs Tier 2 in Python.

Run from the repository root after building the C shared library with a
generated dispatch table:

    sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
    cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
    cmake --build build
    PYTHONPATH=python LD_LIBRARY_PATH=build python3 tests/benchmarks/benchmark_embedding.py
"""

import sys
import time

sys.path.insert(0, "python")

import smc


def bench(name: str, fn, iterations: int) -> float:
    start = time.perf_counter()
    for _ in range(iterations):
        fn()
    elapsed = time.perf_counter() - start
    print(f"{name:40s} {iterations:>10d} calls in {elapsed:>7.3f} s "
          f"({iterations / elapsed:>10.0f} calls/s)")
    return elapsed


def main() -> int:
    count = smc.expr_count()
    if count == 0:
        print("No generated dispatch table; run the generator first.")
        return 1

    # Use the first generated expression for the comparison.
    expr_id = 1
    source = smc.expr_source(expr_id)
    print(f"Benchmarking expression: {source!r}")

    iterations = 100_000

    def tier1():
        smc.eval(source)

    def tier2():
        smc.call(expr_id)

    def pure_python():
        # A hand-optimized Python equivalent for a baseline.
        # This is expression-specific; for the warm corpus we just eval the
        # same arithmetic. A real game would inline the math.
        eval(source)

    t_py = bench("pure Python eval()", pure_python, iterations)
    t_tier1 = bench("SMC Tier 1 (smc.eval)", tier1, iterations)
    t_tier2 = bench("SMC Tier 2 (smc.call)", tier2, iterations)

    print()
    print(f"Tier 1 vs pure Python: {t_py / t_tier1:.2f}x")
    print(f"Tier 2 vs pure Python: {t_py / t_tier2:.2f}x")
    print(f"Tier 2 vs Tier 1:      {t_tier1 / t_tier2:.2f}x")

    return 0


if __name__ == "__main__":
    sys.exit(main())
