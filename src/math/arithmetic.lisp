;;;; arithmetic.lisp
;;;; Arithmetic module: registers basic arithmetic operators.
;;;; This is the simplest math module; it demonstrates the pluggable operator system.

(in-package :self-modifying-calculator)

(defun register-arithmetic-operators ()
  "Register +, -, *, /, and ^ with their standard implementations."
  (register-operator :+ (lambda (&rest args) (apply #'+ args)))
  (register-operator :- (lambda (&rest args) (apply #'- args)))
  (register-operator :* (lambda (&rest args) (apply #'* args)))
  (register-operator :/ (lambda (&rest args) (apply #'/ args)))
  (register-operator :^ (lambda (base exp) (expt base exp))))
