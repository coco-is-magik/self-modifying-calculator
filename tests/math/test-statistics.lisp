;;;; test-statistics.lisp
;;;; Tests for the statistics module.

(in-package :self-modifying-calculator)

(define-test-suite run-statistics-tests
  ;; Mean of 1..5 = 3
  (= 3.0 (evaluate (make-ast :mean (constant-node '(1 2 3 4 5)))))

  ;; Variance of (1,2,3) = 2/3
  (let ((v (evaluate (make-ast :variance (constant-node '(1 2 3))))))
    (< (abs (- v (/ 2.0 3))) 1e-9))

  ;; Standard deviation of (1,1,1) = 0
  (= 0.0 (evaluate (make-ast :std-dev (constant-node '(1 1 1)))))

  ;; Median of odd-length list
  (= 3.0 (evaluate (make-ast :median (constant-node '(1 2 3 4 5)))))

  ;; Median of even-length list
  (= 2.5 (evaluate (make-ast :median (constant-node '(1 2 3 4)))))

  ;; Min, max, sum
  (= 1.0 (evaluate (make-ast :min (constant-node '(1 2 3 4 5)))))
  (= 5.0 (evaluate (make-ast :max (constant-node '(1 2 3 4 5)))))
  (= 15.0 (evaluate (make-ast :sum (constant-node '(1 2 3 4 5))))))
