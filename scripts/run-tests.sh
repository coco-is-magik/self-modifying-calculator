#!/bin/bash
# scripts/run-tests.sh — Run the full test suite.
#
# Usage: ./scripts/run-tests.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

# Ensure SBCL can find its core file
export SBCL_HOME="${SBCL_HOME:-/usr/lib64/sbcl}"

exec sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"tests/load-all-tests.lisp\")" \
  --eval "(sb-ext:exit)"
