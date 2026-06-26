;;;; package.lisp
;;;; Core package definition for the self-modifying calculator.

(defpackage :self-modifying-calculator
  (:use :cl)
  (:nicknames :smc)
  (:export
   ;; AST
   :make-ast
   :ast-p
   :ast-op
   :ast-args
   :constant-node
   :constant-node-p
   :constant-value
   :variable-node
   :variable-node-p
   :variable-name
   :walk-ast
   :map-ast
   :subtrees
   ;; Evaluator
   :evaluate
   :register-operator
   :operator-function
   :variable-value
   :*operator-table*
   :run-calculator
   :main
   :join-strings
   ;; Cache
   :make-cache
   :cache-get
   :cache-set
   :cache-hashtable
   :*global-cache*
   ;; Test runner
   :run-all-tests))
