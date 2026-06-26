;;;; test-linear-algebra.lisp
;;;; Tests for the vector/linear algebra module.

(in-package :self-modifying-calculator)

(define-test-suite run-linear-algebra-tests
  ;; vec3 construction and dot product
  (= 0.8
     (evaluate (make-ast :dot (make-ast :vec3 0 0 1) (make-ast :vec3 0.6 0 0.8))))

  ;; dot product of perpendicular vectors is zero
  (= 0.0
     (evaluate (make-ast :dot (make-ast :vec3 1 0 0) (make-ast :vec3 0 1 0))))

  ;; cross product: i x j = k
  (= 1.0
     (evaluate (make-ast :dot (make-ast :cross (make-ast :vec3 1 0 0) (make-ast :vec3 0 1 0))
                         (make-ast :vec3 0 0 1))))

  ;; norm of unit vector is 1
  (= 1.0
     (evaluate (make-ast :norm (make-ast :vec3 1 0 0))))

  ;; normalization of a vector yields a unit vector
  (let* ((v (make-ast :vec3 3 0 4))
         (unit (evaluate (make-ast :normalize v)))
         (norm (sqrt (+ (* (first unit) (first unit))
                        (* (second unit) (second unit))
                        (* (third unit) (third unit))))))
    (< (abs (- norm 1.0)) 1e-9)))
