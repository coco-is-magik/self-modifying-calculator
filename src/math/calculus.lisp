;;;; calculus.lisp
;;;; Pluggable numerical calculus module for the self-modifying calculator.
;;;;
;;;; Provides :derivative and :integral operators. The operators operate on
;;;; concrete polynomials (coefficient lists) and numeric bounds so they are
;;;; cache-friendly and consistent with the rest of the operator system.

(in-package :self-modifying-calculator)

;;; Numerical derivative of a polynomial at a point

(defun numerical-derivative (coeffs x h)
  "Approximate the derivative of a polynomial at X using a symmetric
   difference quotient with step H. COEFFS is highest-degree first."
  (if (or (null coeffs) (<= (length coeffs) 1))
      0
      (/ (- (evaluate-polynomial coeffs (+ x h))
            (evaluate-polynomial coeffs (- x h)))
         (* 2 h))))

;;; Numerical integration

(defun trapezoidal-integral (coeffs a b n)
  "Estimate the integral of a polynomial from A to B using the trapezoidal
   rule with N subintervals. COEFFS is highest-degree first."
  (when (zerop n)
    (error "Number of subintervals N must be non-zero"))
  (let ((h (/ (- b a) n))
        (sum 0))
    (dotimes (i (1+ n))
      (let ((fi (evaluate-polynomial coeffs (+ a (* i h)))))
        (if (or (zerop i) (= i n))
            (incf sum fi)
            (incf sum (* 2 fi)))))
    (* (/ h 2) sum)))

(defun simpson-integral (coeffs a b n)
  "Estimate the integral of a polynomial from A to B using Simpson's rule.
   N must be an even positive integer."
  (when (or (zerop n) (oddp n))
    (error "Number of subintervals N must be even and non-zero"))
  (let ((h (/ (- b a) n))
        (sum 0))
    (dotimes (i (1+ n))
      (let ((fi (evaluate-polynomial coeffs (+ a (* i h)))))
        (cond ((or (zerop i) (= i n)) (incf sum fi))
              ((oddp i) (incf sum (* 4 fi)))
              (t (incf sum (* 2 fi))))))
    (* (/ h 3) sum)))

;;; Operator registration

(defun register-calculus-operators ()
  "Register the calculus operators with the evaluator."
  (register-operator :derivative (lambda (coeffs x h) (numerical-derivative coeffs x h)))
  (register-operator :integral (lambda (coeffs a b n) (trapezoidal-integral coeffs a b n)))
  (register-operator :simpson-integral (lambda (coeffs a b n) (simpson-integral coeffs a b n)))
  t)

;; Register on load.
(register-calculus-operators)
