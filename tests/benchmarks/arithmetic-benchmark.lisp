;;;; arithmetic-benchmark.lisp
;;;; Simple benchmark comparing conventional evaluation against the
;;;; cache-aware self-modifying calculator.
;;;;
;;;; Usage: load this file after the system and tests are loaded, then run
;;;;   (smc::run-arithmetic-benchmark)

(in-package :self-modifying-calculator)

(defun generate-arithmetic-series (count &optional (seed 42))
  "Generate a deterministic series of COUNT arithmetic expression strings.
   Expressions are randomized but constrained to a small operand/operator set
   so that sub-expressions repeat frequently."
  (let ((rng (make-random-state t))
        (operators '(#\+ #\- #\*)))
    ;; Warm up the RNG deterministically by consuming seed values
    (dotimes (i (mod seed 1000))
      (random 1 rng))
    (loop repeat count
          collect (let* ((a (1+ (random 20 rng)))
                         (b (1+ (random 20 rng)))
                         (op (nth (random (length operators) rng) operators))
                         (expr (format nil "~D~C~D" a op b)))
                    (when (= 0 (random 2 rng))
                      (let* ((c (1+ (random 20 rng)))
                             (op2 (nth (random (length operators) rng) operators)))
                        (setq expr (format nil "(~D~C~D)~C~D" a op b op2 c))))
                    expr))))

(defun conventional-evaluate (expr-string)
  "Evaluate an expression string without caching or self-modification,
   by parsing and then using the non-caching evaluator."
  (evaluate-node (parse expr-string)))

(defun run-arithmetic-benchmark (&key (short 100) (medium 10000) (long 100000) (specialize nil))
  "Run short, medium, and long arithmetic series benchmarks and report speedups.
   If SPECIALIZE is non-nil, enable runtime function specialization for the
   arithmetic operators."
  (when specialize
    (enable-operator-specialization :+ 5)
    (enable-operator-specialization :- 5)
    (enable-operator-specialization :* 5)
    (enable-operator-specialization :/ 5)
    (enable-operator-specialization :^ 5))
  (format t "=== Arithmetic Benchmark (~A) ===~%"
          (if specialize "with specialization" "cache only"))
  (dolist (size (list (cons :short short) (cons :medium medium) (cons :long long)))
    (let* ((label (car size))
           (count (cdr size))
           (series (generate-arithmetic-series count))
           (cache (make-cache)))
      (format t "~%-- ~A series (~D calculations) --~%" label count)

      ;; Conventional baseline
      (let ((start (get-internal-real-time)))
        (dolist (expr series)
          (conventional-evaluate expr))
        (let* ((end (get-internal-real-time))
               (conv-time (/ (- end start) internal-time-units-per-second)))
          (format t "Conventional: ~,4F seconds (~,9F ms/calc)~%"
                  conv-time (* 1000.0 (/ conv-time count)))

          ;; Self-modifying calculator: first pass (cold cache)
          (let ((start2 (get-internal-real-time)))
            (dolist (expr series)
              (evaluate (parse expr) :cache cache))
            (let* ((end2 (get-internal-real-time))
                   (cold-time (/ (- end2 start2) internal-time-units-per-second)))
              (format t "SMC cold:     ~,4F seconds (~,9F ms/calc)~%"
                      cold-time (* 1000.0 (/ cold-time count)))

              ;; Self-modifying calculator: second pass (warm cache)
              (let ((start3 (get-internal-real-time)))
                (dolist (expr series)
                  (evaluate (parse expr) :cache cache))
                (let* ((end3 (get-internal-real-time))
                       (warm-time (/ (- end3 start3) internal-time-units-per-second)))
                  (format t "SMC warm:     ~,4F seconds (~,9F ms/calc) -- speedup vs conventional: ~,2Fx~%"
                          warm-time (* 1000.0 (/ warm-time count))
                          (if (zerop warm-time) :inf (/ conv-time warm-time)))

                  ;; Cache stats
                  (let ((stats (cache-statistics cache)))
                    (format t "Cache stats: size=~D hits=~D misses=~D~%"
                            (getf stats :size)
                            (getf stats :hits)
                            (getf stats :misses))))))))))))
