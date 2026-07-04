;;;; test-ast.lisp
;;;; Tests for the AST module.

(in-package :self-modifying-calculator)

(define-test-suite run-ast-tests
  (= 42 (constant-value (constant-node 42)))
  (constant-node-p (constant-node 42))
  (eq :x (variable-name (variable-node :x)))
  (variable-node-p (variable-node :x))
  (eq :+ (ast-op (make-ast :+ (constant-node 2) (constant-node 3))))
  (= 2 (length (ast-args (make-ast :+ (constant-node 2) (constant-node 3)))))
  (= 3 (ast-size (make-ast :+ (constant-node 2) (constant-node 3))))
  (= 3 (length (subtrees (make-ast :+ (constant-node 2) (constant-node 3)))))
  ;; Regression: clear-ast-intern-table must be exported and functional.
  (progn
    (clear-ast-intern-table)
    (let ((a (constant-node 1))
          (b (constant-node 1)))
      (clear-ast-intern-table)
      (let ((c (constant-node 1)))
        (eq a b)))))
