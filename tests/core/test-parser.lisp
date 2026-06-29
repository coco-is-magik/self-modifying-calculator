;;;; test-parser.lisp
;;;; Tests for the parser module.

(in-package :self-modifying-calculator)

(define-test-suite run-parser-tests
  (= 5 (run-calculator "2+3"))
  (= 10 (run-calculator "2*3+4"))
  (= 20 (run-calculator "(2+3)*4"))
  (= 1024 (run-calculator "2^10"))
  (= 7 (run-calculator "3.5*2"))
  (= -5 (run-calculator "2-7"))
  (= 14 (run-calculator "2+3*4"))
  (= 25 (run-calculator "(2+3)*(4+1)"))
  (= 8 (run-calculator "2^3"))
  (let ((*parse-cache-max-size* 3))
    (clear-parse-cache)
    (parse "1+1")
    (parse "2+2")
    (parse "3+3")
    (parse "4+4")
    (and (= 1 (parse-cache-size))
         (eq (parse "4+4") (gethash "4+4" *parse-cache*))))
  (= 0.0 (evaluate (parse "dot(vec3(1,0,0), vec3(0,1,0))")))
  (= 1.0 (evaluate (parse "norm(vec3(0,0,1))")))
  (= 1.0 (evaluate (parse "sin(1.57079632679)")) 1.0e-5))
