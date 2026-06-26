;;;; test-self-writer.lisp
;;;; Tests for the Level 3 source-code self-writing cache.

(in-package :self-modifying-calculator)

(define-test-suite run-self-writer-tests
  (let ((cache (make-cache)))
    (evaluate (parse "2+3") :cache cache)
    (evaluate (parse "4*5") :cache cache)
    (let ((path (write-cache-as-source cache (merge-pathnames #p"cache/test-generated.lisp" *default-pathname-defaults*))))
      (and (probe-file path)
           (> (load-generated-cache-source path) 0)
           (= 5 (cache-get *global-cache* (parse "2+3")))
           (= 20 (cache-get *global-cache* (parse "4*5")))))))
