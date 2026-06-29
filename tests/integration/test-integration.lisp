;;;; test-integration.lisp
;;;; End-to-end integration tests covering parse -> evaluate -> cache -> save/load workflows.

(in-package :self-modifying-calculator)

(define-test-suite run-integration-tests
  ;; Parse and evaluate with the default global cache
  (let ((*global-cache* (make-cache)))
    (cache-clear *global-cache*)
    (let ((expr1 (parse "2+3"))
          (expr2 (parse "4*5")))
      (= 5 (evaluate expr1))
      (= 20 (evaluate expr2))
      (= 5 (evaluate expr1))  ; cache hit
      (= 2 (cache-size *global-cache*))))

  ;; Evaluate with a fresh local cache, save, then reload into the global cache
  (let ((local-cache (make-cache))
        (temp-path (merge-pathnames #p"cache/test-integration.sexp" *default-pathname-defaults*)))
    (evaluate (parse "3+7") :cache local-cache)
    (evaluate (parse "2*6") :cache local-cache)
    (save-cache local-cache temp-path)
    (let ((*global-cache* (make-cache)))
      (cache-clear *global-cache*)
      (load-cache *global-cache* temp-path)
      (and (= 10 (cache-get *global-cache* (parse "3+7")))
           (= 12 (cache-get *global-cache* (parse "2*6"))))))

  ;; Variable bindings do not corrupt the global cache with stale values
  (let ((cache (make-cache)))
    (= 5 (evaluate (parse "x+2") :cache cache :variables '((:x . 3))))
    (= 9 (evaluate (parse "x+2") :cache cache :variables '((:x . 7))))
    (= 2 (cache-size cache)))

  ;; Algebra operator works through the cache-aware evaluator
  (let ((cache (make-cache)))
    (= 69 (evaluate (make-ast :quadratic 2 3 4 5) :cache cache))
    (let ((roots (evaluate (make-ast :quadratic-roots 1 -5 6) :cache cache)))
      (and (= 2 (length roots)))))

  ;; Statistics operator works through the cache-aware evaluator
  (let ((cache (make-cache)))
    (= 3.0 (evaluate (make-ast :mean (constant-node '(1 2 3 4 5))) :cache cache)))

  ;; Automatic cache persistence loads and saves the global cache
  (let* ((temp-path (merge-pathnames #p"cache/test-auto-persist.sexp" *default-pathname-defaults*))
         (*cache-file-path* temp-path)
         (*auto-persist-cache* t))
    (cache-clear *global-cache*)
    (evaluate (parse "5+5"))
    (maybe-save-global-cache)
    (let ((*global-cache* (make-cache)))
      (cache-clear *global-cache*)
      (maybe-load-global-cache)
      (= 10 (cache-get *global-cache* (parse "5+5")))))

  ;; Unified pipeline configuration can be set to Level 1/2/3
  (let ((*cache-set-hook* nil)
        (*auto-persist-cache* nil)
        (*pipeline-level* 1))
    (configure-optimization 1)
    (= 1 *pipeline-level*)))
