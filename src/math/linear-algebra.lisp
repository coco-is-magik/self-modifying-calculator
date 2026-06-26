;;;; linear-algebra.lisp
;;;; Pluggable vector and linear algebra module for the self-modifying calculator.
;;;;
;;;; Provides :vec3, :dot, :cross, :norm, :normalize. Vectors are represented
;;;; as 3-element lists of numbers, which keeps them cache-friendly and easy
;;;; to print while still supporting list operations.

(in-package :self-modifying-calculator)

(defun vec3 (x y z)
  "Construct a 3D vector from numeric components."
  (list x y z))

(defun vec3-x (v) (first v))
(defun vec3-y (v) (second v))
(defun vec3-z (v) (third v))

(defun dot-product (v1 v2)
  "Compute the dot product of two 3D vectors."
  (+ (* (vec3-x v1) (vec3-x v2))
     (* (vec3-y v1) (vec3-y v2))
     (* (vec3-z v1) (vec3-z v2))))

(defun cross-product (v1 v2)
  "Compute the cross product of two 3D vectors."
  (vec3 (- (* (vec3-y v1) (vec3-z v2)) (* (vec3-z v1) (vec3-y v2)))
        (- (* (vec3-z v1) (vec3-x v2)) (* (vec3-x v1) (vec3-z v2)))
        (- (* (vec3-x v1) (vec3-y v2)) (* (vec3-y v1) (vec3-x v2)))))

(defun vector-norm (v)
  "Compute the Euclidean norm of a vector."
  (sqrt (dot-product v v)))

(defun vector-normalize (v)
  "Return a unit vector in the same direction as V."
  (let ((n (vector-norm v)))
    (if (zerop n)
        (vec3 0 0 0)
        (vec3 (/ (vec3-x v) n) (/ (vec3-y v) n) (/ (vec3-z v) n)))))

(defun register-linear-algebra-operators ()
  "Register the linear algebra operators with the evaluator."
  (register-operator :vec3 (lambda (x y z) (vec3 x y z)))
  (register-operator :dot (lambda (v1 v2) (dot-product v1 v2)))
  (register-operator :cross (lambda (v1 v2) (cross-product v1 v2)))
  (register-operator :norm (lambda (v) (vector-norm v)))
  (register-operator :normalize (lambda (v) (vector-normalize v)))
  t)

;; Register on load.
(register-linear-algebra-operators)
