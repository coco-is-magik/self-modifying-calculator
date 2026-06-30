#!/bin/bash
# scripts/run-demo.sh — Run the focused realistic-renderer demo.
#
# Usage: ./scripts/run-demo.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"scripts/demo.lisp\")" \
  --eval "(sb-ext:exit)"
