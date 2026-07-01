"""Self-Modifying Calculator (SMC) — pure-Python ctypes binding.

This module exposes the stable C ABI v1 to Python without requiring a build
step. It loads ``libsmc.so`` (Linux), ``libsmc.dylib`` (macOS), or
``libsmc.dll`` (Windows) from the usual library search paths.

Example::

    import smc
    print(smc.eval("2 + 3 * 4"))  # 14.0

    ctx = smc.Context(level=2)
    print(ctx.eval("(1 + 2) ^ 3"))  # 27.0
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import c_char_p, c_double, c_float, c_int, c_int64, c_size_t, c_uint32
from pathlib import Path
from typing import Optional, Union

__all__ = [
    "eval",
    "eval_float",
    "eval_int",
    "Context",
    "call",
    "expr_count",
    "expr_arity",
    "expr_source",
    "set_variable",
    "clear_variables",
    "cache_save",
    "cache_load",
    "cache_clear",
    "generate_c_source",
    "SMCError",
]


class SMCError(RuntimeError):
    """Raised when an SMC C API call returns a non-zero error code."""

    def __init__(self, code: int, message: str) -> None:
        self.code = code
        self.message = message
        super().__init__(f"SMC error {code}: {message}")


class _smc_error_t(ctypes.Structure):
    _fields_ = [("code", c_int), ("message", ctypes.c_char * 256)]


class _LibSMC:
    """Lazy-loaded wrapper around the SMC shared library."""

    _instance: Optional["_LibSMC"] = None

    def __new__(cls) -> "_LibSMC":
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._load()
        return cls._instance

    def _load(self) -> None:
        lib_name = self._find_library()

        # If a generated dispatch table is available next to the runtime, load
        # it first with RTLD_GLOBAL so its strong Tier 2 symbols are visible
        # when the runtime library is loaded. The runtime's weak stubs will
        # then resolve to the generated implementations.
        generated_name = self._find_generated_library(lib_name)
        if generated_name:
            ctypes.CDLL(generated_name, mode=ctypes.RTLD_GLOBAL)

        self._lib = ctypes.CDLL(lib_name)

        # Lifecycle
        self._lib.smc_init.restype = c_int
        self._lib.smc_init.argtypes = []
        self._lib.smc_shutdown.restype = c_int
        self._lib.smc_shutdown.argtypes = []
        self._lib.smc_context_create.restype = ctypes.c_void_p
        self._lib.smc_context_create.argtypes = [c_int]
        self._lib.smc_context_destroy.restype = None
        self._lib.smc_context_destroy.argtypes = [ctypes.c_void_p]

        # Global context
        self._lib.smc_set_optimization_level.restype = c_int
        self._lib.smc_set_optimization_level.argtypes = [c_int]
        self._lib.smc_get_optimization_level.restype = c_int
        self._lib.smc_get_optimization_level.argtypes = []

        # Tier 1: eval
        self._lib.smc_eval_double.restype = c_int
        self._lib.smc_eval_double.argtypes = [c_char_p, ctypes.POINTER(c_double)]
        self._lib.smc_eval_double_with.restype = c_int
        self._lib.smc_eval_double_with.argtypes = [
            ctypes.c_void_p,
            c_char_p,
            ctypes.POINTER(c_double),
        ]
        self._lib.smc_eval_float.restype = c_int
        self._lib.smc_eval_float.argtypes = [c_char_p, ctypes.POINTER(c_float)]
        self._lib.smc_eval_float_with.restype = c_int
        self._lib.smc_eval_float_with.argtypes = [
            ctypes.c_void_p,
            c_char_p,
            ctypes.POINTER(c_float),
        ]
        self._lib.smc_eval_int.restype = c_int
        self._lib.smc_eval_int.argtypes = [c_char_p, ctypes.POINTER(c_int64)]
        self._lib.smc_eval_int_with.restype = c_int
        self._lib.smc_eval_int_with.argtypes = [
            ctypes.c_void_p,
            c_char_p,
            ctypes.POINTER(c_int64),
        ]

        # Tier 2: generated-code calls
        self._lib.smc_call_double.restype = c_int
        self._lib.smc_call_double.argtypes = [
            c_uint32,
            ctypes.POINTER(c_double),
            c_size_t,
            ctypes.POINTER(c_double),
        ]

        # Expression metadata
        self._lib.smc_expr_count.restype = c_int
        self._lib.smc_expr_count.argtypes = []
        self._lib.smc_expr_arity.restype = c_size_t
        self._lib.smc_expr_arity.argtypes = [c_uint32]
        self._lib.smc_expr_source.restype = c_char_p
        self._lib.smc_expr_source.argtypes = [c_uint32]

        # Variables
        self._lib.smc_set_variable_double.restype = c_int
        self._lib.smc_set_variable_double.argtypes = [c_char_p, c_double]
        self._lib.smc_set_variable_double_with.restype = c_int
        self._lib.smc_set_variable_double_with.argtypes = [
            ctypes.c_void_p,
            c_char_p,
            c_double,
        ]
        self._lib.smc_clear_variables.restype = c_int
        self._lib.smc_clear_variables.argtypes = []
        self._lib.smc_clear_variables_with.restype = c_int
        self._lib.smc_clear_variables_with.argtypes = [ctypes.c_void_p]

        # Cache
        self._lib.smc_cache_clear.restype = c_int
        self._lib.smc_cache_clear.argtypes = []
        self._lib.smc_cache_clear_with.restype = c_int
        self._lib.smc_cache_clear_with.argtypes = [ctypes.c_void_p]
        self._lib.smc_cache_save.restype = c_int
        self._lib.smc_cache_save.argtypes = [c_char_p]
        self._lib.smc_cache_save_with.restype = c_int
        self._lib.smc_cache_save_with.argtypes = [ctypes.c_void_p, c_char_p]
        self._lib.smc_cache_load.restype = c_int
        self._lib.smc_cache_load.argtypes = [c_char_p]
        self._lib.smc_cache_load_with.restype = c_int
        self._lib.smc_cache_load_with.argtypes = [ctypes.c_void_p, c_char_p]

        # Source generation
        self._lib.smc_generate_c_source.restype = c_int
        self._lib.smc_generate_c_source.argtypes = [c_char_p]
        self._lib.smc_generate_c_source_with.restype = c_int
        self._lib.smc_generate_c_source_with.argtypes = [ctypes.c_void_p, c_char_p]

        # Error handling
        self._lib.smc_error_string.restype = c_char_p
        self._lib.smc_error_string.argtypes = [c_int]
        self._lib.smc_last_error.restype = ctypes.POINTER(_smc_error_t)
        self._lib.smc_last_error.argtypes = []
        self._lib.smc_last_error_with.restype = ctypes.POINTER(_smc_error_t)
        self._lib.smc_last_error_with.argtypes = [ctypes.c_void_p]

        # Initialize the library once at module load time.
        rc = self._lib.smc_init()
        if rc != 0:
            raise SMCError(rc, self._last_error_message())

    @staticmethod
    def _find_library() -> str:
        """Locate the SMC shared library, preferring a local build if present."""
        project_root = Path(__file__).resolve().parent.parent.parent
        candidates = []
        if sys.platform.startswith("linux"):
            candidates = ["libsmc.so"]
        elif sys.platform == "darwin":
            candidates = ["libsmc.dylib"]
        elif sys.platform.startswith("win"):
            candidates = ["libsmc.dll", "smc.dll"]

        # Look next to the package first, then in system paths.
        search_dirs = [project_root / "build", project_root]
        for directory in search_dirs:
            for name in candidates:
                path = directory / name
                if path.exists():
                    return str(path)

        # Fall back to the linker search path.
        for name in candidates:
            try:
                return ctypes.util.find_library(name) or name
            except Exception:
                pass
        return candidates[0]

    @staticmethod
    def _find_generated_library(runtime_path: str) -> Optional[str]:
        """Locate the generated dispatch table next to the runtime library."""
        runtime = Path(runtime_path)
        name = runtime.name
        generated_name: Optional[str] = None
        if name.startswith("libsmc."):
            generated_name = name.replace("libsmc.", "libsmc_generated.", 1)
        elif name.startswith("smc."):
            generated_name = name.replace("smc.", "smc_generated.", 1)
        if not generated_name:
            return None

        # Look in the same directory as the runtime.
        candidate = runtime.with_name(generated_name)
        if candidate.exists():
            return str(candidate)

        # Look in the project build directory.
        project_root = Path(__file__).resolve().parent.parent.parent
        candidate = project_root / "build" / generated_name
        if candidate.exists():
            return str(candidate)

        return None

    def _last_error_message(self) -> str:
        err = self._lib.smc_last_error()
        if err and err.contents.code != 0:
            return err.contents.message.decode("utf-8", errors="replace")
        return "unknown error"

    def _check(self, rc: int, ctx: Optional[ctypes.c_void_p] = None) -> None:
        if rc == 0:
            return
        if ctx is not None:
            err = self._lib.smc_last_error_with(ctx)
        else:
            err = self._lib.smc_last_error()
        message = ""
        if err and err.contents.code != 0:
            message = err.contents.message.decode("utf-8", errors="replace")
        raise SMCError(rc, message or self._last_error_message())


# Module-level helpers for the global context.


def eval(expr: str) -> float:
    """Evaluate a mathematical expression string and return a float."""
    lib = _LibSMC()
    out = c_double()
    lib._check(lib._lib.smc_eval_double(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def eval_float(expr: str) -> float:
    """Evaluate a mathematical expression string and return a float."""
    lib = _LibSMC()
    out = c_float()
    lib._check(lib._lib.smc_eval_float(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def eval_int(expr: str) -> int:
    """Evaluate a mathematical expression string and return an int."""
    lib = _LibSMC()
    out = c_int64()
    lib._check(lib._lib.smc_eval_int(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def set_variable(name: str, value: float) -> None:
    """Bind a variable in the global context."""
    lib = _LibSMC()
    lib._check(
        lib._lib.smc_set_variable_double(name.encode("utf-8"), float(value))
    )


def clear_variables() -> None:
    """Clear all variable bindings in the global context."""
    lib = _LibSMC()
    lib._check(lib._lib.smc_clear_variables())


def cache_save(path: Union[str, os.PathLike]) -> None:
    """Save the global cache to PATH."""
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_save(os.fspath(path).encode("utf-8")))


def cache_load(path: Union[str, os.PathLike]) -> None:
    """Load the global cache from PATH."""
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_load(os.fspath(path).encode("utf-8")))


def cache_clear() -> None:
    """Clear the global cache."""
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_clear())


def generate_c_source(path: Union[str, os.PathLike]) -> None:
    """Generate a C source file from the global cache."""
    lib = _LibSMC()
    lib._check(lib._lib.smc_generate_c_source(os.fspath(path).encode("utf-8")))


def call(expr_id: int, *args: float) -> float:
    """Call a generated expression by stable ID.

    This is the production hot-path API. It requires a runtime built from
    generated C source (Milestone 2).
    """
    lib = _LibSMC()
    argc = len(args)
    c_args = (c_double * argc)(*args)
    out = c_double()
    lib._check(
        lib._lib.smc_call_double(c_uint32(expr_id), c_args, c_size_t(argc), ctypes.byref(out))
    )
    return out.value


def expr_count() -> int:
    """Return the number of expressions in the generated dispatch table."""
    lib = _LibSMC()
    return int(lib._lib.smc_expr_count())


def expr_arity(expr_id: int) -> int:
    """Return the arity (number of free variables) of expression ID."""
    lib = _LibSMC()
    return int(lib._lib.smc_expr_arity(c_uint32(expr_id)))


def expr_source(expr_id: int) -> Optional[str]:
    """Return the original expression string for expression ID, or None."""
    lib = _LibSMC()
    raw = lib._lib.smc_expr_source(c_uint32(expr_id))
    if raw is None:
        return None
    return raw.decode("utf-8", errors="replace")


class Context:
    """An isolated SMC evaluation context.

    Use this when you need thread safety or different optimization levels from
    the global context.
    """

    def __init__(self, level: int = 1) -> None:
        self._lib = _LibSMC()
        self._ctx = self._lib._lib.smc_context_create(level)
        if not self._ctx:
            raise SMCError(-1, "failed to create SMC context")

    def __enter__(self) -> "Context":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def close(self) -> None:
        if self._ctx:
            self._lib._lib.smc_context_destroy(self._ctx)
            self._ctx = None

    def eval(self, expr: str) -> float:
        """Evaluate EXPR in this context and return a float."""
        out = c_double()
        self._lib._check(
            self._lib._lib.smc_eval_double_with(
                self._ctx, expr.encode("utf-8"), ctypes.byref(out)
            ),
            self._ctx,
        )
        return out.value

    def eval_float(self, expr: str) -> float:
        """Evaluate EXPR in this context and return a float."""
        out = c_float()
        self._lib._check(
            self._lib._lib.smc_eval_float_with(
                self._ctx, expr.encode("utf-8"), ctypes.byref(out)
            ),
            self._ctx,
        )
        return out.value

    def eval_int(self, expr: str) -> int:
        """Evaluate EXPR in this context and return an int."""
        out = c_int64()
        self._lib._check(
            self._lib._lib.smc_eval_int_with(
                self._ctx, expr.encode("utf-8"), ctypes.byref(out)
            ),
            self._ctx,
        )
        return out.value

    def set_variable(self, name: str, value: float) -> None:
        """Bind a variable in this context."""
        self._lib._check(
            self._lib._lib.smc_set_variable_double_with(
                self._ctx, name.encode("utf-8"), float(value)
            ),
            self._ctx,
        )

    def clear_variables(self) -> None:
        """Clear all variable bindings in this context."""
        self._lib._check(self._lib._lib.smc_clear_variables_with(self._ctx), self._ctx)

    def cache_save(self, path: Union[str, os.PathLike]) -> None:
        """Save this context's cache to PATH."""
        self._lib._check(
            self._lib._lib.smc_cache_save_with(
                self._ctx, os.fspath(path).encode("utf-8")
            ),
            self._ctx,
        )

    def cache_load(self, path: Union[str, os.PathLike]) -> None:
        """Load a cache into this context from PATH."""
        self._lib._check(
            self._lib._lib.smc_cache_load_with(
                self._ctx, os.fspath(path).encode("utf-8")
            ),
            self._ctx,
        )

    def cache_clear(self) -> None:
        """Clear this context's cache."""
        self._lib._check(self._lib._lib.smc_cache_clear_with(self._ctx), self._ctx)

    def generate_c_source(self, path: Union[str, os.PathLike]) -> None:
        """Generate a C source file from this context's cache."""
        self._lib._check(
            self._lib._lib.smc_generate_c_source_with(
                self._ctx, os.fspath(path).encode("utf-8")
            ),
            self._ctx,
        )

    def call(self, expr_id: int, *args: float) -> float:
        """Call a generated expression by stable ID.

        The generated dispatch table is global, so this uses the same hot-path
        implementation as the module-level :func:`smc.call`.
        """
        argc = len(args)
        c_args = (c_double * argc)(*args)
        out = c_double()
        self._lib._check(
            self._lib._lib.smc_call_double(
                c_uint32(expr_id), c_args, c_size_t(argc), ctypes.byref(out)
            )
        )
        return out.value
