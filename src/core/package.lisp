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
   ;; Statistics
   :mean
   :variance
   :std-dev
   :median
   :sum
   :min
   :max
   ;; Calculus
   :derivative
   :integral
   :simpson-integral
   ;; Algebra
   :quadratic
   :quadratic-roots
   :polynomial-eval
   :factor
   ;; Math module registration
   :register-arithmetic-operators
   :register-linear-algebra-operators
   :register-trigonometry-operators
   :register-algebra-operators
   :register-calculus-operators
   :register-statistics-operators
   ;; Internal utilities exposed for tests / benchmarks
   :run-all-benchmarks
   :evaluate-node
   :enable-operator-specialization
   ;; Test runner
   :run-all-tests))
