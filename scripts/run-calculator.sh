#!/bin/bash
# scripts/run-calculator.sh — Convenience wrapper to run the self-modifying calculator with SBCL.
#
# Usage: ./scripts/run-calculator.sh "<expression>"
# Example: ./scripts/run-calculator.sh "2+3*4"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

# Ensure SBCL can find its core file
export SBCL_HOME="${SBCL_HOME:-/usr/lib64/sbcl}"

exec sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(let ((args (cdr sb-ext:*posix-argv*))) (when args (smc:main args)) (sb-ext:exit))" \
  --end-toplevel-options "$@"
