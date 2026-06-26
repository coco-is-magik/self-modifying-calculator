;;;; test-optimizer.lisp
;;;; Tests for the runtime function specialization optimizer.

(in-package :self-modifying-calculator)

(define-test-suite run-optimizer-tests
  ;; Specialization state records constant operand patterns
  (progn
    (register-operator :opt+ (lambda (a b) (+ a b)))
    (enable-operator-specialization :opt+ 3)
    (dotimes (i 4)
      (evaluate (parse "2+3") :cache (make-cache)))
    (>= (length (specialization-patterns :opt+ 3)) 1))

  ;; Specialized function returns correct results
  (progn
    (register-operator :opt* (lambda (a b) (* a b)))
    (enable-operator-specialization :opt* 2)
    (dotimes (i 3)
      (evaluate (parse "4*5") :cache (make-cache)))
    (= 20 (evaluate (parse "4*5") :cache (make-cache))))

  ;; Unseen patterns still fall back to base function
  (progn
    (register-operator :opts- (lambda (a b) (- a b)))
    (enable-operator-specialization :opts- 10)
    (dotimes (i 3)
      (evaluate (parse "7-2") :cache (make-cache)))
    (= 5 (evaluate (parse "7-2") :cache (make-cache)))))
