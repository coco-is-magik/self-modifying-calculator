;;;; series-generators.lisp
;;;; Deterministic AST series generators for each benchmark category.

(in-package :self-modifying-calculator)

(defun make-rng (seed)
  "Create a deterministic random state from SEED."
  (let ((state (make-random-state t)))
    ;; Consume some random values to seed the state deterministically.
    (dotimes (i (mod seed 1000))
      (random 1 state))
    state))

(defun random-int (rng limit)
  "Return a random integer in [0, LIMIT)."
  (random limit rng))

(defun random-elem (rng list)
  "Return a random element from LIST."
  (nth (random (length list) rng) list))

;;; Arithmetic series

(defun generate-arithmetic-asts (count &optional (seed 42))
  "Generate COUNT arithmetic ASTs with operands 1..20 and operators +, -, *."
  (let ((rng (make-rng seed))
        (operators '(:+ :- :*)))
    (loop repeat count
          collect (let* ((a (1+ (random-int rng 20)))
                         (b (1+ (random-int rng 20)))
                         (c (1+ (random-int rng 20)))
                         (op1 (random-elem rng operators))
                         (op2 (random-elem rng operators)))
                    (make-ast op2 (make-ast op1 a b) c)))))

;;; Linear algebra series

(defun random-vector-ast (rng components)
  "Generate a random :vec3 AST with components drawn from COMPONENTS."
  (make-ast :vec3
            (random-elem rng components)
            (random-elem rng components)
            (random-elem rng components)))

(defun generate-dot-product-asts (count &optional (seed 42))
  "Generate COUNT dot-product ASTs with a fixed light vector and random normals."
  (let ((rng (make-rng seed))
        (components '(-1 0 1))
        (light (make-ast :vec3 0.6 0.0 0.8)))
    (loop repeat count
          collect (make-ast :dot (random-vector-ast rng components) light))))

(defun generate-cross-product-asts (count &optional (seed 42))
  "Generate COUNT cross-product ASTs with random vectors."
  (let ((rng (make-rng seed))
        (components '(-1 0 1)))
    (loop repeat count
          collect (make-ast :cross
                            (random-vector-ast rng components)
                            (random-vector-ast rng components)))))

(defun generate-normalization-asts (count &optional (seed 42))
  "Generate COUNT normalization ASTs with random vectors."
  (let ((rng (make-rng seed))
        (components '(1 2 3)))
    (loop repeat count
          collect (make-ast :normalize (random-vector-ast rng components)))))

;;; Trigonometry series

(defun generate-trig-asts (count &optional (seed 42))
  "Generate COUNT trigonometric ASTs of the form sin(a) + cos(b)."
  (let ((rng (make-rng seed))
        (angles '(0.0 0.5 1.0 1.5 2.0 2.5 3.0 3.5)))
    (loop repeat count
          collect (make-ast :+
                            (make-ast :sin (random-elem rng angles))
                            (make-ast :cos (random-elem rng angles))))))

;;; Polynomial series

(defun generate-polynomial-asts (count &optional (seed 42))
  "Generate COUNT quadratic ASTs of the form a*x^2 + b*x + c."
  (let ((rng (make-rng seed))
        (coeffs '(1 2 3 4 5))
        (xs '(0.0 0.5 1.0 1.5 2.0)))
    (loop repeat count
          collect (let ((a (random-elem rng coeffs))
                        (b (random-elem rng coeffs))
                        (c (random-elem rng coeffs))
                        (x (random-elem rng xs)))
                    (make-ast :+
                              (make-ast :* a (make-ast :^ x 2))
                              (make-ast :* b x)
                              c)))))

;;; Mixed / realistic renderer series

(defun generate-mixed-asts (count &optional (seed 42))
  "Generate COUNT ASTs randomly chosen from arithmetic, dot, trig, and polynomial."
  (let ((rng (make-rng seed)))
    (let ((generators (list (lambda (c) (generate-arithmetic-asts c (random-int rng 10000)))
                            (lambda (c) (generate-dot-product-asts c (random-int rng 10000)))
                            (lambda (c) (generate-trig-asts c (random-int rng 10000)))
                            (lambda (c) (generate-polynomial-asts c (random-int rng 10000))))))
      (loop repeat count
            collect (let* ((gen (random-elem rng generators))
                           (series (funcall gen 1)))
                      (first series))))))

(defun generate-realistic-renderer-asts (count &optional (seed 42))
  "Generate COUNT renderer-style ASTs: dot(N,L) + sin(incident) factor."
  (let ((rng (make-rng seed))
        (components '(-1 0 1))
        (light (make-ast :vec3 0.6 0.0 0.8))
        (angles '(0.0 0.5 1.0 1.5 2.0)))
    (loop repeat count
          collect (let ((normal (random-vector-ast rng components))
                        (angle (random-elem rng angles)))
                    (make-ast :+
                              (make-ast :* (make-ast :dot normal light)
                                        (make-ast :sin angle))
                              0.1)))))
