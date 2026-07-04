;;;; load-all-tests.lisp
;;;; Load all test suites and run them. This is the single entry point for
;;;; running the full test suite.

(in-package :self-modifying-calculator)

(load (merge-pathnames #p"tests/test-runner.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-ast.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-parser.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-evaluator.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-cache.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-optimizer.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-self-writer.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/core/test-linear-algebra.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/math/test-algebra.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/math/test-calculus.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/math/test-statistics.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/integration/test-integration.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/lisp/test-generator.lisp" *default-pathname-defaults*))

;; Regression: all shell scripts must have valid syntax and the linter must run.
(let ((scripts '("scripts/run-calculator.sh"
                  "scripts/run.sh"
                  "scripts/run-tests.sh"
                  "scripts/run-benchmarks.sh"
                  "scripts/run-demo.sh"
                  "scripts/run-linter.sh")))
  (dolist (script scripts)
    (let ((path (merge-pathnames script *default-pathname-defaults*)))
      (assert (probe-file path) () "Missing script: ~A" script)
      (assert (zerop (sb-ext:process-exit-code
                      (sb-ext:run-program "/bin/bash" (list "-n" (namestring path))
                                          :search nil :wait t)))
              () "Shell syntax error in ~A" script))))

(run-all-tests)
