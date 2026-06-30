;;;; ast.lisp
;;;; Abstract Syntax Tree (AST) representation for expressions.
;;;;
;;;; An AST node is one of:
;;;;   - (:constant value)     -- a literal number
;;;;   - (:variable name)      -- a symbolic variable (e.g. x)
;;;;   - (:operator op args...) -- a compound operation

(in-package :self-modifying-calculator)

;;; Canonical AST interning

(defparameter *ast-intern-table* (make-hash-table :test 'equal)
  "Global table that maps AST structure to a canonical AST object.
   Using canonical objects allows EQ-based cache lookups instead of EQUAL.")

(defun intern-ast (node)
  "Return the canonical AST object equal to NODE. If none exists, store NODE
   and return it."
  (or (gethash node *ast-intern-table*)
      (setf (gethash node *ast-intern-table*) node)))

;;; Constructors and accessors

(defun constant-node (value)
  "Create a constant AST node."
  (intern-ast (list :constant value)))

(defun constant-node-p (node)
  (and (consp node) (eq (car node) :constant)))

(defun constant-value (node)
  (second node))

(defun variable-node (name)
  "Create a variable AST node."
  (intern-ast (list :variable name)))

(defun variable-node-p (node)
  (and (consp node) (eq (car node) :variable)))

(defun variable-name (node)
  (second node))

(defun make-ast (op &rest args)
  "Create an operator AST node. Raw numbers are wrapped in constant nodes
   for convenience."
  (intern-ast (cons op (mapcar (lambda (arg)
                                 (if (numberp arg)
                                     (constant-node arg)
                                     arg))
                               args))))

(defun clear-ast-intern-table ()
  "Clear the canonical AST intern table."
  (clrhash *ast-intern-table*)
  nil)

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

;;; ---------------------------------------------------------------------------
;;; AST to string conversion
;;; ---------------------------------------------------------------------------

(defun ast-to-string (node)
  "Convert an AST node back to a human-readable expression string.
   Useful for debugging and for end-to-end parse+eval benchmarks."
  (cond
    ((constant-node-p node)
     (let ((v (constant-value node)))
       (if (integerp v)
           (format nil "~D" v)
           (format nil "~F" v))))
    ((variable-node-p node)
     (string-downcase (symbol-name (variable-name node))))
    ((consp node)
     (let ((op (car node))
           (args (cdr node)))
       (case op
         (:+ (format nil "(~{~A~^ + ~})" (mapcar #'ast-to-string args)))
         (:- (if (= (length args) 1)
                 (format nil "-~A" (ast-to-string (first args)))
                 (format nil "(~{~A~^ - ~})" (mapcar #'ast-to-string args))))
         (:* (format nil "(~{~A~^ * ~})" (mapcar #'ast-to-string args)))
         (:/ (format nil "(~{~A~^ / ~})" (mapcar #'ast-to-string args)))
         (:^ (format nil "(~A ^ ~A)" (ast-to-string (first args)) (ast-to-string (second args))))
         (:vec3 (format nil "vec3(~A, ~A, ~A)"
                        (ast-to-string (first args))
                        (ast-to-string (second args))
                        (ast-to-string (third args))))
         (otherwise
          (format nil "~A(~{~A~^, ~})"
                  (string-downcase (symbol-name op))
                  (mapcar #'ast-to-string args))))))
    (t (error "Invalid AST node: ~A" node))))
