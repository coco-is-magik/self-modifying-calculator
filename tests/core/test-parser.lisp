;;;; test-parser.lisp
;;;; Tests for the parser module.

(in-package :self-modifying-calculator)

(define-test-suite run-parser-tests
  (= 5 (run-calculator "2+3"))
  (= 10 (run-calculator "2*3+4"))
  (= 20 (run-calculator "(2+3)*4"))
  (= 1024 (run-calculator "2^10"))
  (= 7 (run-calculator "3.5*2"))
  (= -5 (run-calculator "2-7"))
  (= 14 (run-calculator "2+3*4"))
  (= 25 (run-calculator "(2+3)*(4+1)"))
  (= 8 (run-calculator "2^3")))
