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
   :ast-to-string
   :constant-node
   :constant-node-p
   :constant-value
   :variable-node
   :variable-node-p
   :variable-name
   :walk-ast
   :map-ast
   :subtrees
   :clear-ast-intern-table
   ;; Evaluator
   :evaluate
   :evaluate-node
   :evaluate-node-cached
   :evaluate-node-uncached
   :register-operator
   :operator-function
   :operator-base-function
   :variable-value
   :set-variable-value
   :*operator-table*
   :*original-operator-functions*
   :run-calculator
   :main
   :join-strings
   ;; Cache
   :make-cache
   :cache-get
   :cache-set
   :cache-contains-p
   :cache-clear
   :cache-size
   :cache-statistics
   :cache-needs-recompile-p
   :mark-cache-compiled
   :*global-cache*
   :*auto-persist-cache*
   :*cache-file-path*
   :save-cache
   :load-cache
   :maybe-load-global-cache
   :maybe-save-global-cache
   ;; Parser
   :parse
   :tokenize
   :clear-parse-cache
   :parse-cache-size
   :*parse-cache*
   :*parse-cache-max-size*
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
   :enable-operator-specialization
   :specialize-operator
   :wrap-operator-with-specialization
   :install-specializing-wrapper
   ;; Test runner
   :run-all-tests
   ;; Unified optimization pipeline
   :configure-optimization
   :*pipeline-level*
   :*pipeline-specialization-threshold*
   :*pipeline-source-rewrite-threshold*
   :*pipeline-source-rewrite-interval*
   :pipeline-enable-specialization
   :pipeline-rewrite-source))
