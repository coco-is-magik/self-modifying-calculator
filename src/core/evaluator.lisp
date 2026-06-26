;;;; evaluator.lisp
;;;; Basic AST evaluator and operator registry.

(in-package :self-modifying-calculator)

;;; Operator registry

(defparameter *operator-table* (make-hash-table :test 'eq)
  "Hash table mapping operator symbols to their implementation functions.")

(defun register-operator (op function)
  "Register a function for the operator OP."
  (setf (gethash op *operator-table*) function))

(defun operator-function (op)
  "Get the implementation function for operator OP, or signal an error."
  (or (gethash op *operator-table*)
      (error "Unknown operator: ~A" op)))

;;; Variable environment

(defparameter *variable-table* (make-hash-table :test 'eq)
  "Hash table mapping variable names to their values.")

(defun variable-value (name)
  "Get the value of a variable, or signal an error if unbound."
  (multiple-value-bind (value found) (gethash name *variable-table*)
    (if found
        value
        (error "Unbound variable: ~A" name))))

(defun set-variable-value (name value)
  "Set the value of a variable."
  (setf (gethash name *variable-table*) value))

;;; Core evaluator

(defun evaluate (node &key (variables nil) (cache *global-cache*))
  "Evaluate an AST node using CACHE. VARIABLES is an alist of (name . value) bindings."
  (let ((*variable-table* (make-hash-table :test 'eq))
        (*global-cache* cache))
    ;; Populate with provided bindings
    (dolist (binding variables)
      (set-variable-value (car binding) (cdr binding)))
    (evaluate-node-cached node)))

(defun evaluate-node-cached (node &optional (cache *global-cache*))
  "Evaluate a single AST node using CACHE. Caches whole and sub-expression results."
  (multiple-value-bind (value found) (cache-get cache node)
    (if found
        (progn
          (incf (cache-hits cache))
          value)
        (progn
          (incf (cache-misses cache))
          (let ((result (evaluate-node-uncached node cache)))
            (cache-set cache node result)
            result)))))

(defun evaluate-node-uncached (node cache)
  "Evaluate NODE without looking it up in CACHE, but still using cache for sub-expressions.
   Rewrites NODE with cached sub-expressions before recursing."
  (let ((rewritten (rewrite-with-cache node cache)))
    (cond
      ((constant-node-p rewritten)
       (constant-value rewritten))
      ((variable-node-p rewritten)
       (variable-value (variable-name rewritten)))
      ((consp rewritten)
       (let ((op (car rewritten))
             (args (cdr rewritten)))
         ;; Recursively evaluate arguments with caching, then apply operator.
         (apply (operator-function op) (mapcar (lambda (arg) (evaluate-node-cached arg cache)) args))))
      (t (error "Invalid AST node: ~A" node)))))

(defun evaluate-node (node)
  "Evaluate a single AST node without caching. Kept for backward compatibility and testing."
  (cond
    ((constant-node-p node)
     (constant-value node))
    ((variable-node-p node)
     (variable-value (variable-name node)))
    ((consp node)
     (let ((op (car node))
           (args (cdr node)))
       (apply (operator-function op) (mapcar #'evaluate-node args))))
    (t (error "Invalid AST node: ~A" node))))

;;; Helper: evaluate with args already evaluated (for operators)

(defun eval-args-then-apply (op args eval-fn)
  "Evaluate all arguments and apply the operator implementation."
  (apply (operator-function op) (mapcar eval-fn args)))

;;; Default operators: arithmetic

(register-operator :+ (lambda (&rest args) (apply #'+ args)))
(register-operator :- (lambda (&rest args) (apply #'- args)))
(register-operator :* (lambda (&rest args) (apply #'* args)))
(register-operator :/ (lambda (&rest args) (apply #'/ args)))
(register-operator :^ (lambda (base exp) (expt base exp)))
