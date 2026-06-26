#!/bin/bash
# run.sh — Convenience wrapper to run the self-modifying calculator with SBCL.
#
# Usage: ./run.sh "<expression>"
# Example: ./run.sh "2+3*4"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR" || exit 1

sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(let ((args (cdr sb-ext:*posix-argv*))) (when args (smc:main args)) (sb-ext:exit))" \
  --end-toplevel-options "$@"
