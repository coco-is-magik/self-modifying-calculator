;;;; test-algebra.lisp
;;;; Tests for the algebra module.

(in-package :self-modifying-calculator)

(define-test-suite run-algebra-tests
  ;; Quadratic evaluation: 2x^2 + 3x + 4 at x = 5
  (= 69 (evaluate (make-ast :quadratic 2 3 4 5)))

  ;; Quadratic roots of x^2 - 5x + 6 = 0 are 2 and 3
  (let ((roots (evaluate (make-ast :quadratic-roots 1 -5 6))))
    (= 2 (length roots)))


  ;; Polynomial eval: 1x^3 + 2x^2 + 3x + 4 at x = 1 = 10
  (= 10 (evaluate (make-ast :polynomial-eval (constant-node '(1 2 3 4)) 1)))

  ;; Factor 12 = 2 * 2 * 3
  (let ((factors (evaluate (make-ast :factor 12))))
    (= 12 (apply #'* factors)))

  ;; Edge case: constant polynomial (zero degree) evaluates to the constant
  (= 5 (evaluate (make-ast :polynomial-eval (constant-node '(5)) 99))))
