#!/bin/bash
# scripts/run-benchmarks.sh — Run the full benchmark suite with improved methodology.
#
# Full output is written to /tmp/smc-benchmark-<timestamp>.out.
# Progress lines ([BENCH]) are shown on stderr during the run.
#
# Usage:
#   ./scripts/run-benchmarks.sh                          # Default: all levels, 10 trials
#   ./scripts/run-benchmarks.sh --level l2               # Only Level 2
#   ./scripts/run-benchmarks.sh --trials 5               # Fewer trials for faster runs
#   ./scripts/run-benchmarks.sh --nice 10                # Lower priority (default 19)
#   ./scripts/run-benchmarks.sh --help                   # Show this help

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
LEVEL=":all"
TRIALS="(list 1 2 3 4 5 6 7 8 9 10)"
NICE_LEVEL=19
OUTFILE="/tmp/smc-benchmark-$(date +%Y%m%d-%H%M%S).out"

function show_help {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Run the self-modifying calculator benchmark suite with rigorous methodology:
- CPU time measured via get-internal-run-time (not wall time)
- GC isolation between phases
- Multiple independent trials at different random seeds
- Median + min/max range reported per category
- Optimization levels run separately (baseline, L1, L1.5, L2)

Full output written to: /tmp/smc-benchmark-<timestamp>.out
Progress lines ([BENCH]) shown on terminal during run.
Process runs at nice -19 to keep machine usable.

Options:
  --level LEVEL   Optimization level: :all, :baseline, :l1, :l1.5, :l2
                  Default: :all (runs all levels, prints comparison table)
  --trials N      Number of independent trials (seeds) per category
                  Default: 10
  --nice N        Niceness level (default 19, lower = higher priority)
  --sweep-domain  Run domain size sweep (not yet implemented)
  --sweep-cache   Run cache size sweep (not yet implemented)
  --help          Show this help message and exit
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      show_help
      exit 0
      ;;
    --level)
      shift
      LEVEL=":$1"
      ;;
    --trials)
      shift
      TRIALS="(loop for i from 1 to $1 collect i)"
      ;;
    --nice)
      shift
      NICE_LEVEL="$1"
      ;;
    --sweep-domain)
      echo "Domain sweep not yet implemented."
      exit 1
      ;;
    --sweep-cache)
      echo "Cache size sweep not yet implemented."
      exit 1
      ;;
    *)
      echo "Unknown option: $1"
      show_help
      exit 1
      ;;
  esac
  shift
done

cd "$PROJECT_DIR" || exit 1

# Ensure SBCL can find its core file
export SBCL_HOME="${SBCL_HOME:-/usr/lib64/sbcl}"

echo "[BENCH] Starting benchmark suite..." >&2
echo "[BENCH] Level: $(echo $LEVEL | tr -d ':')  Trials: $TRIALS" >&2
echo "[BENCH] Output: $OUTFILE" >&2
echo "[BENCH] Niceness: $NICE_LEVEL" >&2

# Run with lowered priority. Full output goes to OUTFILE.
# Only lines starting with [BENCH] are shown on terminal (via stderr).
nice -n "$NICE_LEVEL" \
  sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"tests/benchmarks/series-generators.lisp\")" \
  --eval "(load \"tests/benchmarks/benchmark-framework.lisp\")" \
  --eval "(load \"tests/benchmarks/run-all-benchmarks.lisp\")" \
  --eval "(smc:run-all-benchmarks :level $LEVEL :seeds $TRIALS)" \
  --eval "(sb-ext:exit)" > "$OUTFILE" 2>&1

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
  echo "[BENCH] Complete. Results written to $OUTFILE" >&2
  echo "[BENCH] Summary:" >&2
  grep -E "^(=== Benchmark|Category|Arithmetic|Dot Product|Cross Product|Trig|Polynomial|Mixed|Realistic)" "$OUTFILE" | head -40 >&2
else
  echo "[BENCH] FAILED (exit code $EXIT_CODE). Output in $OUTFILE" >&2
  tail -30 "$OUTFILE" >&2
fi

exit $EXIT_CODE