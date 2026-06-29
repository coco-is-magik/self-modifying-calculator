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

(run-all-tests)
