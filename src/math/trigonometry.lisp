;;;; trigonometry.lisp
;;;; Pluggable trigonometry module for the self-modifying calculator.

(in-package :self-modifying-calculator)

(defun register-trigonometry-operators ()
  "Register sine and cosine operators with the evaluator."
  (register-operator :sin (lambda (x) (sin x)))
  (register-operator :cos (lambda (x) (cos x)))
  t)

;; Register on load.
(register-trigonometry-operators)
