#!/bin/bash
# scripts/run-linter.sh — Basic style and warning check with SBCL.
#
# Usage: ./scripts/run-linter.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

# Ensure SBCL can find its core file
export SBCL_HOME="${SBCL_HOME:-/usr/lib64/sbcl}"

exec sbcl --noinform \
  --eval "(declaim (optimize (debug 3) (safety 2) (speed 2)))" \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(sb-ext:exit)"
