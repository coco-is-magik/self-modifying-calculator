#!/usr/bin/env python3
"""examples/python/hello_smc.py — minimal Python example using the ctypes binding."""

import sys

# Allow running from the repository root without installing the package.
sys.path.insert(0, str(__file__).rsplit("/", 2)[1])

import smc


def main() -> None:
    print("2 + 3 * 4 =", smc.eval("2 + 3 * 4"))
    print("(1 + 2) ^ 3 =", smc.eval("(1 + 2) ^ 3"))
    print("7 // 2 =", smc.eval_int("7 / 2"))

    with smc.Context(level=2) as ctx:
        print("10 - 4 / 2 =", ctx.eval("10 - 4 / 2"))


if __name__ == "__main__":
    main()
