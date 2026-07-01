#!/usr/bin/env python3
"""tests/python/test_smc.py — acceptance tests for the Python ctypes binding."""

import math
import sys
from pathlib import Path

# Allow running from the repository root without installing the package.
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))

import smc


def approx_eq(a: float, b: float, tol: float = 1e-9) -> bool:
    return abs(a - b) < tol


def test_global_eval() -> None:
    assert approx_eq(smc.eval("2 + 3 * 4"), 14.0)
    assert approx_eq(smc.eval("(2 + 3) * 4"), 20.0)
    assert approx_eq(smc.eval("2 ^ 3 ^ 2"), 512.0)
    assert approx_eq(smc.eval("-5 + 3"), -2.0)


def test_eval_variants() -> None:
    assert approx_eq(smc.eval_float("1.5 * 2"), 3.0)
    assert smc.eval_int("7 / 2") == 3


def test_context() -> None:
    with smc.Context(level=2) as ctx:
        assert approx_eq(ctx.eval("10 - 4 / 2"), 8.0)
        assert approx_eq(ctx.eval_float("1.5 * 2"), 3.0)
        assert ctx.eval_int("7 / 2") == 3


def test_context_manual_cleanup() -> None:
    ctx = smc.Context(level=1)
    assert approx_eq(ctx.eval("1 + 1"), 2.0)
    ctx.close()


def test_error_handling() -> None:
    try:
        smc.eval("2 + * 3")
    except smc.SMCError as e:
        assert e.code != 0
        assert "parse" in e.message.lower() or "expected" in e.message.lower()
    else:
        raise AssertionError("expected SMCError for malformed expression")


def test_not_implemented() -> None:
    try:
        smc.set_variable("x", 3.0)
    except smc.SMCError as e:
        assert e.code != 0
    else:
        raise AssertionError("expected SMCError because variables are stubbed")

    try:
        smc.call(42, 1.0, 2.0)
    except smc.SMCError as e:
        assert e.code != 0
    else:
        raise AssertionError("expected SMCError because smc.call is stubbed")


def main() -> int:
    tests = [
        test_global_eval,
        test_eval_variants,
        test_context,
        test_context_manual_cleanup,
        test_error_handling,
        test_not_implemented,
    ]
    for test in tests:
        try:
            test()
        except Exception as e:
            print(f"FAIL: {test.__name__}: {e}")
            return 1
        else:
            print(f"PASS: {test.__name__}")
    print("All Python tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
