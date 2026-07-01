#!/usr/bin/env python3
"""examples/python/renderer_hotpath.py — Python hot-path example.

This example mirrors examples/c/renderer_hotpath.c. It shows how a Python
game or simulation can call generated SMC expressions by stable ID in a tight
loop. The generated dispatch table is produced ahead of time by the SMC
build-time optimizer and linked into a plain C shared library.

Run from the repository root after building the C shared library:

    sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
    cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
    cmake --build build
    PYTHONPATH=python python3 examples/python/renderer_hotpath.py
"""

import sys
import time

# Allow running from the repository root without installing the package.
sys.path.insert(0, "python")

import smc


def main() -> None:
    count = smc.expr_count()
    print(f"Renderer hot path ready. {count} generated expression(s) available.")

    if count == 0:
        print("No generated expressions; nothing to benchmark.")
        return

    iterations = 100_000
    accumulator = 0.0

    start = time.perf_counter()
    for i in range(1, iterations + 1):
        expr_id = (i % count) + 1
        t = i * 0.001
        accumulator += smc.call(expr_id, t)
    elapsed = time.perf_counter() - start

    print(f"Computed {iterations} hot-path calls in {elapsed:.3f} s "
          f"({iterations / elapsed:.0f} calls/s)")
    print(f"Accumulator (prevent DCE): {accumulator:g}")


if __name__ == "__main__":
    main()
