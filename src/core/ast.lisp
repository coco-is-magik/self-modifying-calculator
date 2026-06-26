;;;; ast.lisp
;;;; Abstract Syntax Tree (AST) representation for expressions.
;;;;
;;;; An AST node is one of:
;;;;   - (:constant value)     -- a literal number
;;;;   - (:variable name)      -- a symbolic variable (e.g. x)
;;;;   - (:operator op args...) -- a compound operation

(in-package :self-modifying-calculator)

;;; Constructors and accessors

(defun constant-node (value)
  "Create a constant AST node."
  (list :constant value))

(defun constant-node-p (node)
  (and (consp node) (eq (car node) :constant)))

(defun constant-value (node)
  (second node))

(defun variable-node (name)
  "Create a variable AST node."
  (list :variable name))

(defun variable-node-p (node)
  (and (consp node) (eq (car node) :variable)))

(defun variable-name (node)
  (second node))

(defun make-ast (op &rest args)
  "Create an operator AST node."
  (cons op args))

(defun ast-p (node)
  "Return true if NODE is an AST node (constant, variable, or operator)."
  (and (consp node)
       (or (constant-node-p node)
           (variable-node-p node)
           (symbolp (car node)))))

(defun ast-op (node)
  "Return the operator symbol of an AST node. For constants and variables, return their tag."
  (car node))

(defun ast-args (node)
  "Return the arguments of an AST node."
  (cdr node))

;;; Tree traversal

(defun walk-ast (node fn)
  "Walk the AST, calling FN on each node in pre-order."
  (funcall fn node)
  (when (and (consp node) (not (constant-node-p node)) (not (variable-node-p node)))
    (dolist (child (cdr node))
      (walk-ast child fn))))

(defun map-ast (node fn)
  "Map FN over the AST, returning a new AST with transformed nodes."
  (funcall fn node
           (lambda ()
             (if (or (constant-node-p node) (variable-node-p node))
                 node
                 (cons (car node)
                       (mapcar (lambda (child) (map-ast child fn)) (cdr node)))))))

(defun subtrees (node)
  "Return a list of all sub-trees of NODE, including NODE itself."
  (let ((result '()))
    (walk-ast node (lambda (n) (push n result)))
    (nreverse result)))

;;; Utilities

(defun ast-size (node)
  "Count the number of nodes in an AST."
  (let ((count 0))
    (walk-ast node (lambda (n) (declare (ignore n)) (incf count)))
    count))
