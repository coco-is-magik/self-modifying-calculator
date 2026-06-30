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

(defun generate-arithmetic-asts (count &optional (seed 42) &key (domain-size 20))
  "Generate COUNT arithmetic ASTs with operands 1..DOMAIN-SIZE and operators +, -, *."
  (let ((rng (make-rng seed))
        (operators '(:+ :- :*)))
    (loop repeat count
          collect (let* ((a (1+ (random-int rng domain-size)))
                         (b (1+ (random-int rng domain-size)))
                         (c (1+ (random-int rng domain-size)))
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

(defun generate-dot-product-asts (count &optional (seed 42) &key (component-set '(-1 0 1)))
  "Generate COUNT dot-product ASTs with a fixed light vector and random normals.
   COMPONENT-SET controls the domain of the random normal vector components."
  (let ((rng (make-rng seed))
        (light (make-ast :vec3 0.6 0.0 0.8)))
    (loop repeat count
          collect (make-ast :dot (random-vector-ast rng component-set) light))))

(defun generate-cross-product-asts (count &optional (seed 42) &key (component-set '(-1 0 1)))
  "Generate COUNT cross-product ASTs with random vectors.
   COMPONENT-SET controls the domain of the random vector components."
  (let ((rng (make-rng seed)))
    (loop repeat count
          collect (make-ast :cross
                            (random-vector-ast rng component-set)
                            (random-vector-ast rng component-set)))))

(defun generate-normalization-asts (count &optional (seed 42))
  "Generate COUNT normalization ASTs with random vectors."
  (let ((rng (make-rng seed))
        (components '(1 2 3)))
    (loop repeat count
          collect (make-ast :normalize (random-vector-ast rng components)))))

;;; Matrix-vector multiplication

(defun generate-matrix-multiply-asts (count &optional (seed 42) &key (component-set '(-1 0 1)))
  "Generate COUNT matrix-vector multiply ASTs.
   Uses a fixed 4x4 identity-like matrix and random vectors, so the
   cache sees high sub-expression reuse across the matrix rows."
  (let ((rng (make-rng seed))
        (matrix (make-ast :vec3
                          (make-ast :vec3 1 0 0)
                          (make-ast :vec3 0 1 0)
                          (make-ast :vec3 0 0 1))))
    (loop repeat count
          collect (make-ast :mat4x4-mul matrix (random-vector-ast rng component-set)))))

;;; Trigonometry series

(defun generate-trig-asts (count &optional (seed 42) &key (angle-count 8))
  "Generate COUNT trigonometric ASTs of the form sin(a) + cos(b).
   ANGLE-COUNT controls how many distinct angles are used."
  (let ((rng (make-rng seed))
        (angles (loop for i from 0 below angle-count
                      collect (* i (/ pi (max 1 (1- angle-count)))))))
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

;;; Blinn-Phong shading

(defun generate-blinn-phong-asts (count &optional (seed 42) &key (component-set '(-1 0 1)))
  "Generate COUNT Blinn-Phong shading ASTs:
   ambient + diffuse * dot(N,L) + specular * dot(N,H)^shininess.
   This exercises multiple operators sharing sub-expressions."
  (let* ((rng (make-rng seed))
         (light (make-ast :vec3 0.6 0.0 0.8))
         (view (make-ast :vec3 0.0 0.0 1.0))
         (half-angle (make-ast :normalize (make-ast :vec3-add light view)))
         (shininess '(2 4 8 16 32)))
    (loop repeat count
          collect (let ((normal (random-vector-ast rng component-set))
                       (shiny (random-elem rng shininess)))
                    (make-ast :+
                              0.1
                              (make-ast :*
                                        0.7
                                        (make-ast :dot normal light))
                              (make-ast :*
                                        0.3
                                        (make-ast :^
                                                  (make-ast :dot normal half-angle)
                                                  shiny)))))))

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
  "Generate COUNT renderer-style ASTs: dot(N,L) * sin(incident) factor."
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

;;; End-to-end string-parsed benchmark

(defun generate-expression-strings (count &optional (seed 42))
  "Generate COUNT arithmetic expression strings (not ASTs).
   For end-to-end parse+eval benchmarking."
  (let ((asts (generate-arithmetic-asts count seed)))
    (mapcar #'ast-to-string asts)))