;;;; self-modifying-calculator.asd
;;;; ASDF system definition for the Self-Modifying Calculator.

(asdf:defsystem #:self-modifying-calculator
  :description "A self-modifying calculator that caches expressions and sub-expressions for repeated computation."
  :author "coco-is-magik"
  :license  "MIT"
  :version  "0.1.0"
  :serial   t
  :components ((:file "src/core/package")
               (:file "src/core/ast")
               (:file "src/core/cache")
               (:file "src/core/matcher")
               (:file "src/core/evaluator")
               (:file "src/core/optimizer")
               (:file "src/core/dispatch-compiler")
               (:file "src/core/self-writer")
               (:file "src/interface/parser")
               (:file "src/math/arithmetic")
               (:file "src/math/algebra")
               (:file "src/math/calculus")
               (:file "src/math/linear-algebra")
               (:file "src/math/trigonometry")
               (:file "src/math/statistics")
               (:file "src/main")))
