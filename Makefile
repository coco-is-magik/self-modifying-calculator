# Makefile for Self-Modifying Calculator (SMC)
# Provides common development and testing shortcuts.
#
# Note: SBCL_HOME must be set if sbcl.core is in a non-standard location:
#   export SBCL_HOME=/usr/lib64/sbcl

SBCL_HOME ?= /usr/lib64/sbcl
SBCL      := SBCL_HOME=$(SBCL_HOME) sbcl --noinform
ASDF_EVAL := $(SBCL) --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
                     --eval "(asdf:load-system :self-modifying-calculator)"
.PHONY: all test test-c demo bench lint calc generate build-c clean help

all: test

help:
	@echo "SMC Development Targets"
	@echo "  make test       — Run all Lisp tests"
	@echo "  make test-c     — Run all C tests (requires build-c first)"
	@echo "  make demo       — Run the realistic-renderer demo"
	@echo "  make bench      — Run benchmarks (may take a long time)"
	@echo "  make lint       — Lint-check Lisp source with safety/debug settings"
	@echo "  make calc EXPR=<expr> — Evaluate an expression"
	@echo "  make generate   — Generate C source from the current SMC cache"
	@echo "  make build-c    — Configure and build C libraries + tests"
	@echo "  make build-c-sanitize — Build C libraries with ASan/UBSan"
	@echo "  make clean      — Remove build artifacts"

test:
	$(SBCL) \
	  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
	  --eval "(asdf:load-system :self-modifying-calculator)" \
	  --eval "(load \"tests/load-all-tests.lisp\")" \
	  --eval "(sb-ext:exit)"

test-c:
	cd build && ctest --output-on-failure

demo:
	$(SBCL) \
	  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
	  --eval "(asdf:load-system :self-modifying-calculator)" \
	  --eval "(load \"scripts/demo.lisp\")" \
	  --eval "(sb-ext:exit)"

bench:
	./scripts/run-benchmarks.sh

lint:
	$(SBCL) \
	  --eval "(declaim (optimize (debug 3) (safety 2) (speed 2)))" \
	  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
	  --eval "(asdf:load-system :self-modifying-calculator)" \
	  --eval "(sb-ext:exit)"

calc:
	@if [ -z "$(EXPR)" ]; then \
	  echo "Usage: make calc EXPR='<expression>'"; \
	  echo "  e.g. make calc EXPR='2+3*4'"; \
	  exit 1; \
	fi
	$(SBCL) \
	  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
	  --eval "(asdf:load-system :self-modifying-calculator)" \
	  --eval "(smc:main '(\"$(EXPR)\"))" \
	  --eval "(sb-ext:exit)"

generate:
	mkdir -p build
	SBCL_HOME=$(SBCL_HOME) sbcl --script scripts/generate-c-source.lisp build/smc_generated.c

build-c: generate
	cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
	cmake --build build

build-c-sanitize: generate
	cmake -B build-sanitize -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c -DSMC_SANITIZE=ON
	cmake --build build-sanitize

clean:
	rm -rf build/ build-sanitize/
	rm -f cache/generated/*.lisp
	find . -name '*.fasl' -delete
