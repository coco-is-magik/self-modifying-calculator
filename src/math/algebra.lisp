;;;; algebra.lisp
;;;; Pluggable algebra module for the self-modifying calculator.
;;;;
;;;; Provides :quadratic, :quadratic-roots, :polynomial-eval, and :factor
;;;; operators. Quadratic and polynomial evaluation use the operator registry
;;;; so they benefit from expression caching and function specialization.

(in-package :self-modifying-calculator)

;;; Polynomial evaluation

(defun evaluate-polynomial (coeffs x)
  "Evaluate a polynomial at X. COEFFS is a list of coefficients from highest
   degree to lowest degree, e.g., '(1 0 0) represents x^2."
  (if (null coeffs)
      0
      (let ((result 0))
        (dolist (c coeffs)
          (setq result (+ (* result x) c)))
        result)))

(defun polynomial-derivative (coeffs)
  "Return the coefficients of the derivative of the polynomial represented
   by COEFFS (highest degree first)."
  (if (or (null coeffs) (<= (length coeffs) 1))
      '(0)
      (let ((n (1- (length coeffs)))
            (result '()))
        (dolist (c coeffs)
          (when (> n 0)
            (push (* c n) result)
            (decf n)))
        (nreverse result))))

;;; Quadratic helpers

(defun quadratic-value (a b c x)
  "Evaluate ax^2 + bx + c at X."
  (+ (* a x x) (* b x) c))

(defun quadratic-roots (a b c)
  "Return the real roots of ax^2 + bx + c = 0 as a list. If the discriminant
   is negative, return the complex roots as (real imag) pairs. If A is zero,
   the equation is linear and the single root is returned."
  (if (zerop a)
      (if (zerop b)
          (error "Not an equation: both a and b are zero")
          (list (/ (- c) b)))
      (let ((discriminant (- (* b b) (* 4 a c)))
            (2a (* 2 a)))
        (if (>= discriminant 0)
            (let ((sqrt-d (sqrt discriminant)))
              (list (/ (- (- b) sqrt-d) 2a)
                    (/ (+ (- b) sqrt-d) 2a)))
            (let ((sqrt-d (sqrt (- discriminant)))
                  (-b/2a (/ (- b) 2a)))
              (list (list -b/2a (/ sqrt-d 2a))
                    (list -b/2a (/ (- sqrt-d) 2a))))))))

;;; Simplistic factoring placeholder

(defun factor-number (n)
  "Return the prime factorization of N as a list of prime factors."
  (if (<= n 1)
      (list n)
      (let ((factors '())
            (remaining n))
        (loop for p from 2 while (<= (* p p) remaining) do
          (loop while (zerop (mod remaining p)) do
            (push p factors)
            (setq remaining (/ remaining p))))
        (when (> remaining 1)
          (push remaining factors))
        (nreverse factors))))

;;; Operator registration

(defun register-algebra-operators ()
  "Register the algebra operators with the evaluator."
  (register-operator :quadratic (lambda (a b c x) (quadratic-value a b c x)))
  (register-operator :quadratic-roots (lambda (a b c) (quadratic-roots a b c)))
  (register-operator :polynomial-eval (lambda (coeffs x) (evaluate-polynomial coeffs x)))
  (register-operator :factor (lambda (n) (factor-number n)))
  t)

;; Register on load.
(register-algebra-operators)
