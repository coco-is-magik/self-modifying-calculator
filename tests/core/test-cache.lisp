;;;; test-cache.lisp
;;;; Tests for the cache and cache-aware evaluator.

(in-package :self-modifying-calculator)

(define-test-suite run-cache-tests
  ;; Basic cache operations
  (let ((cache (make-cache)))
    (cache-set cache (make-ast :+ (constant-node 2) (constant-node 3)) 5)
    (= 5 (cache-get cache (make-ast :+ (constant-node 2) (constant-node 3))))
    (= 1 (cache-size cache))
    (cache-clear cache)
    (= 0 (cache-size cache)))

  ;; Variables are not cached
  (let ((cache (make-cache)))
    (cache-set cache (make-ast :+ (variable-node :x) (constant-node 3)) 99)
    (= 0 (cache-size cache)))

  ;; Repeated evaluation caches results
  (let ((cache (make-cache))
        (expr (parse "2+3")))
    (= 5 (evaluate expr :cache cache))
    (= 5 (evaluate expr :cache cache))
    (= 2 (cache-size cache))
    (= 2 (cache-hits cache))
    (= 1 (cache-misses cache)))

  ;; Sub-expression reuse
  (let ((cache (make-cache)))
    (evaluate (parse "(2+3)*4") :cache cache)
    (evaluate (parse "(2+3)*5") :cache cache)
    (= 4 (cache-size cache))
    (> (cache-hits cache) 0))

  ;; Cache statistics
  (let ((cache (make-cache)))
    (evaluate (parse "2+2") :cache cache)
    (let ((stats (cache-statistics cache)))
      (and (getf stats :size)
           (getf stats :hits)
           (getf stats :misses)))))
