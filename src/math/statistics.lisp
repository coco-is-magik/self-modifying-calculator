;;;; statistics.lisp
;;;; Pluggable statistics module for the self-modifying calculator.
;;;;
;;;; Provides :mean, :variance, :std-dev, :median, :min, :max, and :sum
;;;; operators. All operators accept a list of numeric values and return a
;;;; single numeric result, which keeps them cache-friendly.

(in-package :self-modifying-calculator)

;;; Statistical helpers

(defun arithmetic-mean (values)
  "Compute the arithmetic mean of VALUES."
  (if (null values)
      (error "Cannot compute mean of empty list")
      (/ (apply #'+ values) (length values))))

(defun population-variance (values)
  "Compute the population variance of VALUES."
  (if (null values)
      (error "Cannot compute variance of empty list")
      (let ((mean (arithmetic-mean values)))
        (/ (apply #'+ (mapcar (lambda (x) (expt (- x mean) 2)) values))
           (length values)))))

(defun population-std-dev (values)
  "Compute the population standard deviation of VALUES."
  (sqrt (population-variance values)))

(defun statistical-median (values)
  "Compute the median of VALUES."
  (if (null values)
      (error "Cannot compute median of empty list")
      (let* ((sorted (sort (copy-list values) #'<))
             (n (length sorted))
             (mid (floor n 2)))
        (if (oddp n)
            (nth mid sorted)
            (/ (+ (nth (1- mid) sorted) (nth mid sorted)) 2)))))

;;; Operator registration

(defun register-statistics-operators ()
  "Register the statistics operators with the evaluator."
  (register-operator :mean (lambda (values) (arithmetic-mean values)))
  (register-operator :variance (lambda (values) (population-variance values)))
  (register-operator :std-dev (lambda (values) (population-std-dev values)))
  (register-operator :median (lambda (values) (statistical-median values)))
  (register-operator :min (lambda (values) (apply #'min values)))
  (register-operator :max (lambda (values) (apply #'max values)))
  (register-operator :sum (lambda (values) (apply #'+ values)))
  t)

;; Register on load.
(register-statistics-operators)
