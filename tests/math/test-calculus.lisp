;;;; test-calculus.lisp
;;;; Tests for the numerical calculus module.

(in-package :self-modifying-calculator)

(define-test-suite run-calculus-tests

  ;; Derivative of x^2 at x = 3 is approximately 6
  (let ((d (evaluate (make-ast :derivative (constant-node '(1 0 0)) 3 1e-6))))
    (< (abs (- d 6.0)) 1e-4))

  ;; Integral of x^2 from 0 to 1 = 1/3
  (let ((result (evaluate (make-ast :integral (constant-node '(1 0 0)) 0 1 1000))))
    (< (abs (- result (/ 1.0 3))) 1e-6))

  ;; Simpson's rule for x^2 from 0 to 1 is exact for quadratics
  (let ((result (evaluate (make-ast :simpson-integral (constant-node '(1 0 0)) 0 1 100))))
    (< (abs (- result (/ 1.0 3))) 1e-12)))
