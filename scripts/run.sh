#!/bin/bash
# scripts/run.sh — Backward-compatible alias for scripts/run-calculator.sh.
#
# Usage: ./scripts/run.sh "<expression>"
# Example: ./scripts/run.sh "2+3*4"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure SBCL can find its core file
export SBCL_HOME="${SBCL_HOME:-/usr/lib64/sbcl}"

exec "$SCRIPT_DIR/run-calculator.sh" "$@"
