;;;; test-evaluator.lisp
;;;; Tests for the evaluator and operator registry.

(in-package :self-modifying-calculator)

(define-test-suite run-evaluator-tests
  (= 5 (evaluate (make-ast :+ (constant-node 2) (constant-node 3))))
  (= 6 (evaluate (make-ast :* (constant-node 2) (constant-node 3))))
  (= -1 (evaluate (make-ast :- (constant-node 2) (constant-node 3))))
  (= 2 (evaluate (make-ast :/ (constant-node 6) (constant-node 3))))
  (= 8 (evaluate (make-ast :^ (constant-node 2) (constant-node 3))))
  (= 7 (evaluate (make-ast :+ (variable-node :x) (constant-node 2))
                 :variables '((:x . 5))))
  (eq :impl (progn
              (register-operator :custom (lambda (x y) (+ (* x 2) y)))
              (evaluate (make-ast :custom (constant-node 3) (constant-node 4))))))
