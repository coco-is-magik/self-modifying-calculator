#!/usr/bin/env python3
"""Self-Modifying Calculator (SMC) — pure-Python ctypes binding.

This module exposes the stable C ABI v1 to Python without requiring a build
step. It loads ``libsmc.so`` (Linux), ``libsmc.dylib`` (macOS), or
``libsmc.dll`` (Windows) from the usual library search paths.

Example::

    import smc
    print(smc.eval("2 + 3 * 4"))  # 14.0

    ctx = smc.Context(level=2)
    print(ctx.eval("(1 + 2) ^ 3"))  # 27.0

    # Tier 2 hot path with observability
    print(smc.call(1, 3.0))
    print(smc.get_stats())
    smc.reset_stats()
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import c_char_p, c_double, c_float, c_int, c_int64, c_size_t, c_uint32, c_uint64
from pathlib import Path
from typing import Optional, Union

__all__ = [
    "abi_version",
    "runtime_kind",
    "features",
    "SMC_FEATURE_ARTIFACT_CACHE",
    "SMC_FEATURE_STATE_TRACKING",
    "SMC_OK",
    "SMC_ERR_NOT_FOUND",
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
    "artifact_configure",
    "artifact_lookup",
    "artifact_store",
    "artifact_remove",
    "artifact_clear",
    "artifact_get_stats",
    "artifact_reset_stats",
    "state_configure",
    "state_changed",
    "state_clear",
    "state_get_stats",
    "state_reset_stats",
    "get_stats",
    "reset_stats",
    "SMCError",
    "SMCStats",
    "SMCArtifactStats",
    "SMCStateStats",
]

SMC_FEATURE_ARTIFACT_CACHE = 0x01
SMC_FEATURE_STATE_TRACKING = 0x02
SMC_OK = 0
SMC_ERR_NOT_FOUND = 1


class SMCError(RuntimeError):
    """Raised when an SMC C API call returns a non-zero error code."""

    def __init__(self, code: int, message: str) -> None:
        self.code = code
        self.message = message
        super().__init__(f"SMC error {code}: {message}")


class _smc_error_t(ctypes.Structure):
    _fields_ = [("code", c_int), ("message", ctypes.c_char * 256)]


class _smc_artifact_config_t(ctypes.Structure):
    _fields_ = [
        ("max_entries", c_size_t),
        ("max_key_size", c_size_t),
        ("max_value_size", c_size_t),
        ("memory_budget_bytes", c_size_t),
    ]


class _smc_state_config_t(ctypes.Structure):
    _fields_ = [
        ("max_entries", c_size_t),
        ("max_key_size", c_size_t),
        ("max_state_size", c_size_t),
        ("memory_budget_bytes", c_size_t),
    ]


class _smc_stats_t(ctypes.Structure):
    _fields_ = [
        ("total_calls", c_uint64),
        ("generated_hits", c_uint64),
        ("fallback_evals", c_uint64),
        ("invalid_ids", c_uint64),
        ("arity_errors", c_uint64),
        ("invalid_calls", c_uint64),
        ("parse_errors", c_uint64),
        ("last_error_code", c_int),
    ]


class _smc_artifact_stats_t(ctypes.Structure):
    _fields_ = [
        ("lookups", c_uint64),
        ("hits", c_uint64),
        ("misses", c_uint64),
        ("stores", c_uint64),
        ("updates", c_uint64),
        ("evictions", c_uint64),
        ("removes", c_uint64),
        ("clears", c_uint64),
        ("bytes_stored", c_uint64),
        ("bytes_returned", c_uint64),
    ]


class _smc_state_stats_t(ctypes.Structure):
    _fields_ = [
        ("checks", c_uint64),
        ("changed", c_uint64),
        ("unchanged", c_uint64),
        ("stores", c_uint64),
        ("evictions", c_uint64),
        ("bytes_compared", c_uint64),
    ]


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

        generated_name = self._find_generated_library(lib_name)
        if generated_name:
            self._lib = ctypes.CDLL(lib_name, mode=ctypes.RTLD_GLOBAL)
            ctypes.CDLL(generated_name, mode=ctypes.RTLD_GLOBAL)
        else:
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

        # Introspection
        self._lib.smc_abi_version.restype = c_int
        self._lib.smc_abi_version.argtypes = []
        self._lib.smc_runtime_kind.restype = c_char_p
        self._lib.smc_runtime_kind.argtypes = []
        self._lib.smc_features.restype = c_uint32
        self._lib.smc_features.argtypes = []

        # Tier 1: eval
        self._lib.smc_eval_double.restype = c_int
        self._lib.smc_eval_double.argtypes = [c_char_p, ctypes.POINTER(c_double)]
        self._lib.smc_eval_double_with.restype = c_int
        self._lib.smc_eval_double_with.argtypes = [ctypes.c_void_p, c_char_p, ctypes.POINTER(c_double)]
        self._lib.smc_eval_float.restype = c_int
        self._lib.smc_eval_float.argtypes = [c_char_p, ctypes.POINTER(c_float)]
        self._lib.smc_eval_float_with.restype = c_int
        self._lib.smc_eval_float_with.argtypes = [ctypes.c_void_p, c_char_p, ctypes.POINTER(c_float)]
        self._lib.smc_eval_int.restype = c_int
        self._lib.smc_eval_int.argtypes = [c_char_p, ctypes.POINTER(c_int64)]
        self._lib.smc_eval_int_with.restype = c_int
        self._lib.smc_eval_int_with.argtypes = [ctypes.c_void_p, c_char_p, ctypes.POINTER(c_int64)]

        # Tier 2: generated-code calls
        self._lib.smc_call_double.restype = c_int
        self._lib.smc_call_double.argtypes = [c_uint32, ctypes.POINTER(c_double), c_size_t, ctypes.POINTER(c_double)]

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
        self._lib.smc_set_variable_double_with.argtypes = [ctypes.c_void_p, c_char_p, c_double]
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

        # Artifact cache
        self._lib.smc_artifact_configure.restype = c_int
        self._lib.smc_artifact_configure.argtypes = [ctypes.c_void_p, ctypes.POINTER(_smc_artifact_config_t)]
        self._lib.smc_artifact_lookup.restype = c_int
        self._lib.smc_artifact_lookup.argtypes = [ctypes.c_void_p, ctypes.c_void_p, c_size_t, ctypes.c_void_p, c_size_t, ctypes.POINTER(c_size_t)]
        self._lib.smc_artifact_store.restype = c_int
        self._lib.smc_artifact_store.argtypes = [ctypes.c_void_p, ctypes.c_void_p, c_size_t, ctypes.c_void_p, c_size_t]
        self._lib.smc_artifact_remove.restype = c_int
        self._lib.smc_artifact_remove.argtypes = [ctypes.c_void_p, ctypes.c_void_p, c_size_t]
        self._lib.smc_artifact_clear.restype = c_int
        self._lib.smc_artifact_clear.argtypes = [ctypes.c_void_p]
        self._lib.smc_artifact_get_stats.restype = c_int
        self._lib.smc_artifact_get_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_smc_artifact_stats_t)]
        self._lib.smc_artifact_reset_stats.restype = c_int
        self._lib.smc_artifact_reset_stats.argtypes = [ctypes.c_void_p]

        # State tracking
        self._lib.smc_state_configure.restype = c_int
        self._lib.smc_state_configure.argtypes = [ctypes.c_void_p, ctypes.POINTER(_smc_state_config_t)]
        self._lib.smc_state_changed.restype = c_int
        self._lib.smc_state_changed.argtypes = [ctypes.c_void_p, ctypes.c_void_p, c_size_t, ctypes.c_void_p, c_size_t, ctypes.POINTER(c_int)]
        self._lib.smc_state_clear.restype = c_int
        self._lib.smc_state_clear.argtypes = [ctypes.c_void_p]
        self._lib.smc_state_get_stats.restype = c_int
        self._lib.smc_state_get_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_smc_state_stats_t)]
        self._lib.smc_state_reset_stats.restype = c_int
        self._lib.smc_state_reset_stats.argtypes = [ctypes.c_void_p]

        # Observability
        self._lib.smc_get_stats.restype = c_int
        self._lib.smc_get_stats.argtypes = [ctypes.POINTER(_smc_stats_t)]
        self._lib.smc_reset_stats.restype = c_int
        self._lib.smc_reset_stats.argtypes = []

        # Error handling
        self._lib.smc_last_error.restype = ctypes.POINTER(_smc_error_t)
        self._lib.smc_last_error.argtypes = []
        self._lib.smc_last_error_with.restype = ctypes.POINTER(_smc_error_t)
        self._lib.smc_last_error_with.argtypes = [ctypes.c_void_p]

        rc = self._lib.smc_init()
        if rc != 0:
            raise SMCError(rc, self._last_error_message())

    @staticmethod
    def _find_library() -> str:
        project_root = Path(__file__).resolve().parent.parent.parent
        if sys.platform.startswith("linux"):
            candidates = ["libsmc.so"]
        elif sys.platform == "darwin":
            candidates = ["libsmc.dylib"]
        elif sys.platform.startswith("win"):
            candidates = ["libsmc.dll", "smc.dll"]
        else:
            candidates = ["libsmc.so"]

        for directory in [project_root / "build", project_root]:
            for name in candidates:
                path = directory / name
                if path.exists():
                    return str(path)
        return candidates[0]

    @staticmethod
    def _find_generated_library(runtime_path: str) -> Optional[str]:
        runtime = Path(runtime_path)
        name = runtime.name
        if name.startswith("libsmc."):
            generated_name = name.replace("libsmc.", "libsmc_generated.", 1)
        elif name.startswith("smc."):
            generated_name = name.replace("smc.", "smc_generated.", 1)
        else:
            return None
        candidate = runtime.with_name(generated_name)
        if candidate.exists():
            return str(candidate)
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
        err = self._lib.smc_last_error_with(ctx) if ctx else self._lib.smc_last_error()
        message = ""
        if err and err.contents.code != 0:
            message = err.contents.message.decode("utf-8", errors="replace")
        raise SMCError(rc, message or self._last_error_message())


# Module-level functions

def abi_version() -> int:
    lib = _LibSMC()
    return int(lib._lib.smc_abi_version())


def runtime_kind() -> str:
    lib = _LibSMC()
    raw = lib._lib.smc_runtime_kind()
    return raw.decode("utf-8", errors="replace") if raw else "unknown"


def features() -> int:
    lib = _LibSMC()
    return int(lib._lib.smc_features())


def eval(expr: str) -> float:
    lib = _LibSMC()
    out = c_double()
    lib._check(lib._lib.smc_eval_double(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def eval_float(expr: str) -> float:
    lib = _LibSMC()
    out = c_float()
    lib._check(lib._lib.smc_eval_float(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def eval_int(expr: str) -> int:
    lib = _LibSMC()
    out = c_int64()
    lib._check(lib._lib.smc_eval_int(expr.encode("utf-8"), ctypes.byref(out)))
    return out.value


def set_variable(name: str, value: float) -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_set_variable_double(name.encode("utf-8"), float(value)))


def clear_variables() -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_clear_variables())


def cache_save(path: Union[str, os.PathLike]) -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_save(os.fspath(path).encode("utf-8")))


def cache_load(path: Union[str, os.PathLike]) -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_load(os.fspath(path).encode("utf-8")))


def cache_clear() -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_cache_clear())


def generate_c_source(path: Union[str, os.PathLike]) -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_generate_c_source(os.fspath(path).encode("utf-8")))


def call(expr_id: int, *args: float) -> float:
    lib = _LibSMC()
    argc = len(args)
    c_args = (c_double * argc)(*args)
    out = c_double()
    lib._check(lib._lib.smc_call_double(c_uint32(expr_id), c_args, c_size_t(argc), ctypes.byref(out)))
    return out.value


def expr_count() -> int:
    lib = _LibSMC()
    return int(lib._lib.smc_expr_count())


def expr_arity(expr_id: int) -> int:
    lib = _LibSMC()
    return int(lib._lib.smc_expr_arity(c_uint32(expr_id)))


def expr_source(expr_id: int) -> Optional[str]:
    lib = _LibSMC()
    raw = lib._lib.smc_expr_source(c_uint32(expr_id))
    return raw.decode("utf-8", errors="replace") if raw else None


class SMCStats:
    def __init__(self, raw: "_smc_stats_t") -> None:
        self.total_calls = raw.total_calls
        self.generated_hits = raw.generated_hits
        self.fallback_evals = raw.fallback_evals
        self.invalid_ids = raw.invalid_ids
        self.arity_errors = raw.arity_errors
        self.invalid_calls = raw.invalid_calls
        self.parse_errors = raw.parse_errors
        self.last_error_code = raw.last_error_code

    def __repr__(self) -> str:
        return f"SMCStats(total_calls={self.total_calls}, generated_hits={self.generated_hits}, ...)"


class SMCArtifactStats:
    def __init__(self, raw) -> None:
        self.lookups = raw.lookups
        self.hits = raw.hits
        self.misses = raw.misses
        self.stores = raw.stores
        self.updates = raw.updates
        self.evictions = raw.evictions
        self.removes = raw.removes
        self.clears = raw.clears
        self.bytes_stored = raw.bytes_stored
        self.bytes_returned = raw.bytes_returned

    def __repr__(self) -> str:
        return f"SMCArtifactStats(lookups={self.lookups}, hits={self.hits}, ...)"


class SMCStateStats:
    def __init__(self, raw) -> None:
        self.checks = raw.checks
        self.changed = raw.changed
        self.unchanged = raw.unchanged
        self.stores = raw.stores
        self.evictions = raw.evictions
        self.bytes_compared = raw.bytes_compared

    def __repr__(self) -> str:
        return f"SMCStateStats(checks={self.checks}, changed={self.changed}, ...)"


class Context:
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
        out = c_double()
        rc = self._lib._lib.smc_eval_double_with(self._ctx, expr.encode("utf-8"), ctypes.byref(out))
        self._lib._check(rc, self._ctx)
        return out.value

    def eval_float(self, expr: str) -> float:
        out = c_float()
        rc = self._lib._lib.smc_eval_float_with(self._ctx, expr.encode("utf-8"), ctypes.byref(out))
        self._lib._check(rc, self._ctx)
        return out.value

    def eval_int(self, expr: str) -> int:
        out = c_int64()
        rc = self._lib._lib.smc_eval_int_with(self._ctx, expr.encode("utf-8"), ctypes.byref(out))
        self._lib._check(rc, self._ctx)
        return out.value

    def set_variable(self, name: str, value: float) -> None:
        rc = self._lib._lib.smc_set_variable_double_with(self._ctx, name.encode("utf-8"), float(value))
        self._lib._check(rc, self._ctx)

    def clear_variables(self) -> None:
        rc = self._lib._lib.smc_clear_variables_with(self._ctx)
        self._lib._check(rc, self._ctx)

    def cache_save(self, path):
        rc = self._lib._lib.smc_cache_save_with(self._ctx, os.fspath(path).encode("utf-8"))
        self._lib._check(rc, self._ctx)

    def cache_load(self, path):
        rc = self._lib._lib.smc_cache_load_with(self._ctx, os.fspath(path).encode("utf-8"))
        self._lib._check(rc, self._ctx)

    def cache_clear(self) -> None:
        rc = self._lib._lib.smc_cache_clear_with(self._ctx)
        self._lib._check(rc, self._ctx)

    def generate_c_source(self, path):
        rc = self._lib._lib.smc_generate_c_source_with(self._ctx, os.fspath(path).encode("utf-8"))
        self._lib._check(rc, self._ctx)


def artifact_configure(ctx, config) -> None:
    lib = _LibSMC()
    c_config = _smc_artifact_config_t()
    if config:
        c_config.max_entries = config.get("max_entries", 0)
        c_config.max_key_size = config.get("max_key_size", 0)
        c_config.max_value_size = config.get("max_value_size", 0)
        c_config.memory_budget_bytes = config.get("memory_budget_bytes", 0)
    rc = lib._lib.smc_artifact_configure(ctx._ctx if isinstance(ctx, Context) else ctx, ctypes.byref(c_config))
    lib._check(rc)


def artifact_lookup(ctx, key: bytes, value_capacity: int) -> tuple:
    lib = _LibSMC()
    out = ctypes.create_string_buffer(value_capacity)
    out_size = c_uint64()
    rc = lib._lib.smc_artifact_lookup(ctx._ctx if isinstance(ctx, Context) else ctx, key, len(key), ctypes.byref(out), value_capacity, ctypes.byref(out_size))
    lib._check(rc)
    return bytes(out[:out_size.value]), out_size.value


def artifact_store(ctx, key: bytes, value: bytes) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_artifact_store(ctx._ctx if isinstance(ctx, Context) else ctx, key, len(key), value, len(value))
    lib._check(rc)


def artifact_remove(ctx, key: bytes) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_artifact_remove(ctx._ctx if isinstance(ctx, Context) else ctx, key, len(key))
    lib._check(rc)


def artifact_clear(ctx) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_artifact_clear(ctx._ctx if isinstance(ctx, Context) else ctx)
    lib._check(rc)


def artifact_get_stats(ctx) -> SMCArtifactStats:
    lib = _LibSMC()
    raw = _smc_artifact_stats_t()
    rc = lib._lib.smc_artifact_get_stats(ctx._ctx if isinstance(ctx, Context) else ctx, ctypes.byref(raw))
    lib._check(rc)
    return SMCArtifactStats(raw)


def artifact_reset_stats(ctx) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_artifact_reset_stats(ctx._ctx if isinstance(ctx, Context) else ctx)
    lib._check(rc)


def state_configure(ctx, config=None) -> None:
    lib = _LibSMC()
    c_config = _smc_state_config_t()
    if config:
        c_config.max_entries = config.get("max_entries", 0)
        c_config.max_key_size = config.get("max_key_size", 0)
        c_config.max_state_size = config.get("max_state_size", 0)
        c_config.memory_budget_bytes = config.get("memory_budget_bytes", 0)
    rc = lib._lib.smc_state_configure(ctx._ctx if isinstance(ctx, Context) else ctx, ctypes.byref(c_config))
    lib._check(rc)


def state_changed(ctx, key: bytes, state: bytes) -> bool:
    lib = _LibSMC()
    out_changed = c_int()
    rc = lib._lib.smc_state_changed(ctx._ctx if isinstance(ctx, Context) else ctx, key, len(key), state, len(state), ctypes.byref(out_changed))
    lib._check(rc)
    return bool(out_changed.value)


def state_clear(ctx) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_state_clear(ctx._ctx if isinstance(ctx, Context) else ctx)
    lib._check(rc)


def state_get_stats(ctx) -> SMCStateStats:
    lib = _LibSMC()
    raw = _smc_state_stats_t()
    rc = lib._lib.smc_state_get_stats(ctx._ctx if isinstance(ctx, Context) else ctx, ctypes.byref(raw))
    lib._check(rc)
    return SMCStateStats(raw)


def state_reset_stats(ctx) -> None:
    lib = _LibSMC()
    rc = lib._lib.smc_state_reset_stats(ctx._ctx if isinstance(ctx, Context) else ctx)
    lib._check(rc)


def get_stats() -> SMCStats:
    lib = _LibSMC()
    raw = _smc_stats_t()
    lib._check(lib._lib.smc_get_stats(ctypes.byref(raw)))
    return SMCStats(raw)


def reset_stats() -> None:
    lib = _LibSMC()
    lib._check(lib._lib.smc_reset_stats())