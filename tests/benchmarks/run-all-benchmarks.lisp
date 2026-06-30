;;;; run-all-benchmarks.lisp
;;;; Entry point that runs all per-category benchmarks and prints summary tables.
;;;;
;;;; Supports running at multiple optimization levels and aggregating results
;;;; across multiple random seeds for statistical rigor.
;;;;
;;;; Progress lines prefixed with [BENCH] are emitted to standard output.
;;;; The run-benchmarks.sh script filters these to show terminal progress.

(in-package :self-modifying-calculator)

;;; ---------------------------------------------------------------------------
;;; Category name functions
;;; ---------------------------------------------------------------------------

(defun arithmetic-name (count seed)
  (declare (ignore count seed))
  "Arithmetic")

(defun dot-product-name (count seed)
  (declare (ignore count seed))
  "Dot Product")

(defun cross-product-name (count seed)
  (declare (ignore count seed))
  "Cross Product")

(defun trig-name (count seed)
  (declare (ignore count seed))
  "Trig")

(defun polynomial-name (count seed)
  (declare (ignore count seed))
  "Polynomial")

(defun mixed-name (count seed)
  (declare (ignore count seed))
  "Mixed")

(defun realistic-renderer-name (count seed)
  (declare (ignore count seed))
  "Realistic Renderer")

(defun matrix-multiply-name (count seed)
  (declare (ignore count seed))
  "Matrix Multiply")

(defun blinn-phong-name (count seed)
  (declare (ignore count seed))
  "Blinn-Phong")

(defun e2e-name (count seed)
  (declare (ignore count seed))
  "End-to-End Parse+Eval")

;;; List of (name-function . generator-function) pairs for each category.
(defparameter *benchmark-categories*
  (list (cons #'arithmetic-name #'generate-arithmetic-asts)
        (cons #'dot-product-name #'generate-dot-product-asts)
        (cons #'cross-product-name #'generate-cross-product-asts)
        (cons #'trig-name #'generate-trig-asts)
        (cons #'polynomial-name #'generate-polynomial-asts)
        (cons #'mixed-name #'generate-mixed-asts)
        (cons #'realistic-renderer-name #'generate-realistic-renderer-asts)
        (cons #'matrix-multiply-name #'generate-matrix-multiply-asts)
        (cons #'blinn-phong-name #'generate-blinn-phong-asts))
  "Alist of (name-fn . gen-fn) for each benchmark category.")

(defparameter *e2e-category*
  (cons #'e2e-name #'generate-expression-strings)
  "End-to-end (parse+eval) benchmark category.")

;;; ---------------------------------------------------------------------------
;;; Per-level runner
;;; ---------------------------------------------------------------------------

(defun run-level (level counts &key (seeds *benchmark-default-seeds*) (cache-max-size 0))
  "Run all benchmark categories at a given optimization LEVEL.
   COUNTS is a list of series lengths (e.g., '(100 10000 100000)).
   Returns a list of TRIAL-AGGREGATE structs, one per (category × count).
   Emits [BENCH] progress lines for the run-benchmarks.sh script."
  (let ((nseeds (length seeds)))
    (loop for count in counts
          append
          (loop for (name-fn . gen-fn) in *benchmark-categories*
                for name = (funcall name-fn count (first seeds))
                for gen-wrapper = (lambda (c s)
                                    (values (funcall name-fn c s)
                                            (funcall gen-fn c s)))
                do
                (format t "[BENCH] Level ~A: ~A (~D) ~D seeds~%"
                        level name count nseeds)
                collect
                (run-trials gen-wrapper count
                            :level level
                            :seeds seeds
                            :cache-max-size cache-max-size)))))

(defun configure-level (level)
  "Configure the evaluator for the given optimization LEVEL.
   Returns the evaluator to its base state first, then applies the level's setup."
  (ensure-base-operator-table)
  (case level
    (:baseline
     (format *trace-output* "Configured level: BASELINE (no caching)~%"))
    (:l1
     (format *trace-output* "Configured level: L1 (cache only, no dispatch, no spec)~%"))
    (:l1.5
     (format *trace-output* "Configured level: L1.5 (cache + compiled dispatch)~%"))
    (:l2
     (format *trace-output* "Configured level: L2 (cache + dispatch + specialization)~%"))))

;;; ---------------------------------------------------------------------------
;;; Main entry points
;;; ---------------------------------------------------------------------------

(defun run-all-benchmarks (&key (short 100) (medium 10000) (long 100000)
                                (level :all)
                                (seeds *benchmark-default-seeds*)
                                (cache-max-size 0)
                                (e2e nil))
  "Run all benchmark categories at the specified optimization level(s).

   LEVEL can be:
     :all       -- run at baseline, l1, l1.5, and l2, then print comparison
     :baseline  -- no caching, raw evaluate-node
     :l1        -- cache only, no compiled dispatch, no specialization
     :l1.5      -- cache + compiled dispatch
     :l2        -- cache + compiled dispatch + specialization

   If E2E is true, also run the end-to-end parse+eval benchmark.

   Returns a list of all TRIAL-AGGREGATE structs generated."
  (let* ((counts (list short medium long))
         (levels-to-run (case level
                          (:all '(:baseline :l1 :l1.5 :l2))
                          (t (list level))))
         (all-aggregates '()))

    (gc-and-settle)

    (dolist (lev levels-to-run)
      (configure-level lev)

      ;; For Level 2, run a warmup pass first so specialization thresholds
      ;; are crossed before any timed measurements.
      (when (eq lev :l2)
        (format t "[BENCH] L2 warmup pass...~%")
        (let ((warmup-series (generate-mixed-asts (car counts))))
          (setup-warmup-for-level-2 warmup-series))
        (gc-and-settle))

      (format t "[BENCH] Running at level ~A...~%" lev)
      (let ((aggregates (run-level lev counts :seeds seeds :cache-max-size cache-max-size)))
        (print-aggregate-summary-table aggregates)
        (setf all-aggregates (append all-aggregates aggregates)))

      ;; End-to-end parse+eval benchmark
      (when e2e
        (format t "[BENCH] Running end-to-end parse+eval at level ~A...~%" lev)
        (let ((e2e-agg (run-trials-e2e
                        (lambda (c s)
                          (values (funcall (car *e2e-category*) c s)
                                  (funcall (cdr *e2e-category*) c s)))
                        (car counts)
                        :level lev
                        :seeds seeds
                        :cache-max-size cache-max-size)))
          (print-aggregate-summary-table (list e2e-agg))
          (push e2e-agg all-aggregates))))

    ;; Print per-level comparison across all categories
    (when (eq level :all)
      (print-level-comparison all-aggregates))

    ;; Print machine-readable sexp output
    (format t "~%;;; machine-readable sexp output~%")
    (print-aggregates-as-sexp all-aggregates)

    all-aggregates))

;;; ---------------------------------------------------------------------------
;;; Domain size sweep (Phase B)
;;; ---------------------------------------------------------------------------

(defun run-domain-sweep (level &key (count 10000)
                                (seeds *benchmark-default-seeds*)
                                (arithmetic-sizes '(5 10 20 50 100))
                                (component-sets '((-1 0 1) (-5 5) (-50 50))))
  "Run a domain size sweep: vary the value range and measure performance.
   Compares how speedup changes as the domain of random values expands."
  (format t "[BENCH] Domain size sweep at level ~A (~D calculations)~%" level count)
  (format t "[BENCH] Arithmetic domain sizes: ~A~%" arithmetic-sizes)
  (format t "[BENCH] Vector component sets: ~A~%" component-sets)
  (gc-and-settle)
  (configure-level level)

  ;; Arithmetic sweep: vary domain-size
  (format t "~%~%=== Domain Sweep: Arithmetic ===~%")
  (format t "~15A ~10A ~10A~%" "Domain" "Conv(ms)" "Warm(ms)")
  (format t "~A~%" (make-string 40 :initial-element #\-))
  (dolist (ds arithmetic-sizes)
    (let* ((series (generate-arithmetic-asts count 42 :domain-size ds))
           (trial (run-single-trial (format nil "Arithmetic (dom=~D)" ds) series
                                    :count count :level level :seed 42)))
      (format t "~15D ~10,6F ~10,6F~%" ds
              (* 1000.0 (/ (benchmark-trial-conventional-time trial) count))
              (* 1000.0 (/ (benchmark-trial-smc-warm-time trial) count)))))

  ;; Vector sweep: vary component sets
  (format t "~%~%=== Domain Sweep: Dot Product ===~%")
  (format t "~25A ~10A ~10A~%" "Component Set" "Conv(ms)" "Warm(ms)")
  (format t "~A~%" (make-string 50 :initial-element #\-))
  (dolist (cs component-sets)
    (let* ((series (generate-dot-product-asts count 42 :component-set cs))
           (trial (run-single-trial (format nil "Dot (comp=~A)" cs) series
                                    :count count :level level :seed 42)))
      (format t "~25A ~10,6F ~10,6F~%" cs
              (* 1000.0 (/ (benchmark-trial-conventional-time trial) count))
              (* 1000.0 (/ (benchmark-trial-smc-warm-time trial) count))))))

(defun run-cache-size-sweep (level &key (count 10000)
                                    (seeds *benchmark-default-seeds*)
                                    (cache-sizes '(100 1000 10000 0)))
  "Run a cache size sweep: vary the max-size parameter and measure performance.
   0 means unlimited cache. Iterates over CACHE-SIZES list."
  (format t "[BENCH] Cache size sweep at level ~A (~D calculations)~%" level count)
  (format t "[BENCH] Cache sizes: ~A~%" cache-sizes)
  (gc-and-settle)
  (configure-level level)

  (dolist (max-size cache-sizes)
    (format t "~%~%=== Cache Size: ~D (~A) ===~%"
            max-size (if (zerop max-size) "unlimited" (format nil "max ~D" max-size)))
    (let ((aggregates
           (run-level level (list count)
                      :seeds seeds
                      :cache-max-size max-size)))
      (print-aggregate-summary-table aggregates)))

  ;; Print overview comparison
  (format t "~%~%=== Cache Size Comparison ===~%")
  (format t "~25A" "Category")
  (dolist (cs cache-sizes)
    (format t " ~12A" (if (zerop cs) "Unlim" (format nil "Max ~D" cs))))
  (format t "~%")
  (format t "~A~%" (make-string 80 :initial-element #\-))
  (dolist (pair *benchmark-categories*)
    (let ((name-fn (car pair)))
      (format t "~25A" (funcall name-fn count (first seeds)))
      (dolist (cs cache-sizes)
        (let* ((level-agg
                (car (run-level level (list count)
                                :seeds (list (first seeds))
                                :cache-max-size cs)))
               (sp (if level-agg (trial-aggregate-warm-speedup-median level-agg) 0.0)))
          (if (or (zerop sp) (>= sp most-positive-fixnum))
              (format t " ~12A" "INF")
              (format t " ~11,2F×" sp))))
      (format t "~%"))))
