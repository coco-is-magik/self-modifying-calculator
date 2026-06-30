#!/bin/bash
# scripts/run-benchmarks.sh — Run the full benchmark suite.
#
# Usage: ./scripts/run-benchmarks.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR" || exit 1

sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"tests/benchmarks/benchmark-framework.lisp\")" \
  --eval "(load \"tests/benchmarks/series-generators.lisp\")" \
  --eval "(load \"tests/benchmarks/run-all-benchmarks.lisp\")" \
  --eval "(smc:run-all-benchmarks)" \
  --eval "(sb-ext:exit)"
