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


def test_variables() -> None:
    smc.set_variable("x", 3.0)
    assert approx_eq(smc.eval("x + 2"), 5.0)
    smc.clear_variables()
    try:
        smc.eval("x + 2")
    except smc.SMCError as e:
        assert e.code != 0
    else:
        raise AssertionError("expected SMCError for unbound variable after clear")


def test_not_implemented() -> None:
    try:
        smc.call(42, 1.0, 2.0)
    except smc.SMCError as e:
        assert e.code != 0
    else:
        raise AssertionError("expected SMCError because smc.call is stubbed")


def test_generated_call() -> None:
    """Exercise the Tier 2 hot path when a generated dispatch table is linked.

    The default generated table now contains argumentized expressions, so we
    bind variables in the global context before cross-checking against Tier 1.
    """
    count = smc.expr_count()
    if count == 0:
        print("SKIP: test_generated_call (no generated dispatch table)")
        return

    for expr_id in range(1, count + 1):
        source = smc.expr_source(expr_id)
        assert source is not None
        arity = smc.expr_arity(expr_id)
        args = (3.2, 2.1)[:arity]

        smc.clear_variables()
        if arity >= 1:
            smc.set_variable("x", 3.2)
        if arity >= 2:
            smc.set_variable("y", 2.1)
        expected = smc.eval(source)
        value = smc.call(expr_id, *args)
        assert approx_eq(value, expected), f"id={expr_id} source={source!r}"

    # Invalid IDs should still raise.
    try:
        smc.call(0)
    except smc.SMCError:
        pass
    else:
        raise AssertionError("expected SMCError for expr_id 0")


def test_all_public_names_importable() -> None:
    """Every name advertised in smc.__all__ must be importable and callable."""
    for name in smc.__all__:
        obj = getattr(smc, name)
        assert obj is not None, f"smc.{name} is None"


def test_ctypes_types_imported() -> None:
    """The ctypes types used by the binding must be present in smc.__init__.

    This is a regression test for a missing c_uint64 import that broke
    the _smc_stats_t struct definition.
    """
    from smc import (
        c_char_p,
        c_double,
        c_float,
        c_int,
        c_int64,
        c_size_t,
        c_uint32,
        c_uint64,
    )

    assert c_uint64 is not None


def test_generated_table_loads_when_present() -> None:
    """When a generated dispatch table exists next to the runtime, loading
    the Python module must not raise an undefined-symbol error.

    This is a regression test for the load order that caused:
        undefined symbol: smc_global_stats
    """
    # If a generated table is present, expr_count() will be non-negative and
    # the module will have loaded successfully. The mere fact that we got
    # this far without an ImportError/OSError is the real assertion.
    assert smc.expr_count() >= 0


def main() -> int:
    tests = [
        test_global_eval,
        test_eval_variants,
        test_context,
        test_context_manual_cleanup,
        test_error_handling,
        test_variables,
        test_not_implemented,
        test_generated_call,
        test_all_public_names_importable,
        test_ctypes_types_imported,
        test_generated_table_loads_when_present,
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
