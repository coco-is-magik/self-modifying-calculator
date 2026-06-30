;;;; benchmark-framework.lisp
;;;; Common timing, reporting, and execution utilities for the benchmark suite.
;;;;
;;;; Supports multiple optimization levels:
;;;;   :baseline -- no caching, raw evaluate-node
;;;;   :l1       -- cache via EQ hash table, no compiled dispatch, no specialization
;;;;   :l1.5     -- cache + compiled dispatch (auto-engages for small caches)
;;;;   :l2       -- cache + compiled dispatch + operator specialization
;;;;
;;;; Each (category × size × level × seed) combination is an independent trial.
;;;; Trials are aggregated across seeds to report median + range.

(in-package :self-modifying-calculator)

;;; ---------------------------------------------------------------------------
;;; Configuration
;;; ---------------------------------------------------------------------------

(defparameter *benchmark-default-seeds* (loop for i from 1 to 10 collect i)
  "Default set of seeds for multi-trial benchmarks.")

(defparameter *benchmark-repeats*
  '((100 . 100)      ; short series: repeat 100× to accumulate measurable time
    (10000 . 10)     ; medium series: repeat 10×
    (100000 . 1))    ; long series: repeat once
  "Alist mapping series length to repeat count for accumulating measurable time.")

;;; ---------------------------------------------------------------------------
;;; Timing utilities
;;; ---------------------------------------------------------------------------

(defun measure-cpu-time (thunk)
  "Measure the CPU (run) time taken by THUNK using get-internal-run-time.
   Returns seconds as a float. This excludes GC pauses and kernel scheduling
   because get-internal-run-time only counts CPU time spent in the thread."
  (let ((start (get-internal-run-time)))
    (funcall thunk)
    (let ((end (get-internal-run-time)))
      (/ (- end start) internal-time-units-per-second))))

(defun gc-and-settle ()
  "Run a full GC and pause briefly to let the system settle.
   Call before each timed phase to prevent allocation from one phase
   polluting another."
  (sb-ext:gc :full t)
  (sleep 0.05))

(defun compute-repeats (count)
  "Return the number of repetitions for a series of COUNT calculations."
  (or (cdr (assoc count *benchmark-repeats*)) 1))

;;; ---------------------------------------------------------------------------
;;; Unique-ratio computation
;;; ---------------------------------------------------------------------------

(defun unique-ratio (series)
  "Return the fraction of unique ASTs in SERIES, as a float in [0, 1]."
  (if (zerop (length series))
      0.0
      (/ (length (remove-duplicates series :test #'equal))
         (length series) 1.0)))

;;; ---------------------------------------------------------------------------
;;; Result structures
;;; ---------------------------------------------------------------------------

(defstruct benchmark-trial
  "Result of a single benchmark trial (one category × size × level × seed)."
  category          ; string name
  count             ; number of calculations in this run
  level             ; :baseline, :l1, :l1.5, or :l2
  seed              ; integer seed used for generation
  conventional-time ; CPU seconds for non-caching evaluate-node
  smc-cold-time     ; CPU seconds for first pass with caching
  smc-warm-time     ; CPU seconds for second pass with caching
  cache-stats       ; plist of cache statistics after all runs
  unique-ratio      ; fraction of unique ASTs in the series [0, 1]
  ;; per-phase GC times for diagnostics
  conventional-gc-time
  smc-cold-gc-time
  smc-warm-gc-time)

(defstruct trial-aggregate
  "Aggregated statistics across multiple trials at the same (category, size, level)."
  category
  count
  level
  trial-count
  ;; Median + range for each metric
  conventional-time-median
  conventional-time-min
  conventional-time-max
  smc-cold-time-median
  smc-cold-time-min
  smc-cold-time-max
  smc-warm-time-median
  smc-warm-time-min
  smc-warm-time-max
  cold-speedup-median
  cold-speedup-min
  cold-speedup-max
  warm-speedup-median
  warm-speedup-min
  warm-speedup-max
  cache-stats        ; from the median trial (representative)
  unique-ratio       ; same for all trials (seed-independent when series is large)
  ;; GC overhead
  conventional-gc-median
  smc-cold-gc-median
  smc-warm-gc-median)

;;; ---------------------------------------------------------------------------
;;; Median and range helpers
;;; ---------------------------------------------------------------------------

(defun sorted (list)
  "Return a sorted copy of LIST (ascending, using <)."
  (sort (copy-list list) #'<))

(defun median (sorted-list)
  "Return the median of a sorted list of numbers."
  (let* ((len (length sorted-list))
         (mid (floor len 2)))
    (if (oddp len)
        (nth mid sorted-list)
        (/ (+ (nth (1- mid) sorted-list)
              (nth mid sorted-list))
           2.0))))

(defun aggregate-trials (trials)
  "Aggregate a list of BENCHMARK-TRIAL structs into a TRIAL-AGGREGATE.
   Reports median speedup over convention (for warm and cold) and
   min/max range across all trials."
  (let* ((conv-times (sorted (mapcar #'benchmark-trial-conventional-time trials)))
         (cold-times (sorted (mapcar #'benchmark-trial-smc-cold-time trials)))
         (warm-times (sorted (mapcar #'benchmark-trial-smc-warm-time trials)))
         (cold-speedups (sorted (mapcar (lambda (trial)
                                          (let ((conv (benchmark-trial-conventional-time trial))
                                                (cold (benchmark-trial-smc-cold-time trial)))
                                            (if (zerop cold) most-positive-fixnum (/ conv cold))))
                                        trials)))
         (warm-speedups (sorted (mapcar (lambda (trial)
                                          (let ((conv (benchmark-trial-conventional-time trial))
                                                (warm (benchmark-trial-smc-warm-time trial)))
                                            (if (zerop warm) most-positive-fixnum (/ conv warm))))
                                        trials))))
    (labels ((min-of (sorted) (first sorted))
             (max-of (sorted) (first (last sorted)))
             (med-of (sorted) (median sorted)))
      (make-trial-aggregate
       :category (benchmark-trial-category (first trials))
       :count (benchmark-trial-count (first trials))
       :level (benchmark-trial-level (first trials))
       :trial-count (length trials)
       :conventional-time-median (med-of conv-times)
       :conventional-time-min (min-of conv-times)
       :conventional-time-max (max-of conv-times)
       :smc-cold-time-median (med-of cold-times)
       :smc-cold-time-min (min-of cold-times)
       :smc-cold-time-max (max-of cold-times)
       :smc-warm-time-median (med-of warm-times)
       :smc-warm-time-min (min-of warm-times)
       :smc-warm-time-max (max-of warm-times)
       :cold-speedup-median (med-of cold-speedups)
       :cold-speedup-min (min-of cold-speedups)
       :cold-speedup-max (max-of cold-speedups)
       :warm-speedup-median (med-of warm-speedups)
       :warm-speedup-min (min-of warm-speedups)
       :warm-speedup-max (max-of warm-speedups)
       :cache-stats (benchmark-trial-cache-stats (first trials))
       :unique-ratio (benchmark-trial-unique-ratio (first trials))
       :conventional-gc-median (median (sorted (mapcar #'benchmark-trial-conventional-gc-time trials)))
       :smc-cold-gc-median (median (sorted (mapcar #'benchmark-trial-smc-cold-gc-time trials)))
       :smc-warm-gc-median (median (sorted (mapcar #'benchmark-trial-smc-warm-gc-time trials)))))))

;;; ---------------------------------------------------------------------------
;;; Core trial runner
;;; ---------------------------------------------------------------------------

(defun run-single-trial (name ast-series &key (count 1000) (level :l1) (seed 42)
                                                  (cache-max-size 0))
  "Run a single benchmark trial at a given optimization level.
   Returns a BENCHMARK-TRIAL struct.

   LEVEL determines evaluation mode:
     :baseline -- evaluate-node (no caching at all)
     :l1       -- evaluate with cache, no compiled dispatch
     :l1.5     -- evaluate with cache + compiled dispatch
     :l2       -- evaluate with cache + compiled dispatch + operator specialization

   Before each timed phase, a full GC is run to isolate allocation effects.
   CPU time (not wall time) is measured to exclude kernel scheduling noise."
  (declare (ignore seed))  ; seed is recorded metadata; generators use it externally
  (let* ((repeats (compute-repeats count))
         (unique-frac (unique-ratio ast-series)))

    (labels ((measure-cached (cache passes)
               "Run PASSES iterations of the series through `evaluate`.
                Returns the average CPU time per pass."
               (/ (measure-cpu-time
                    (lambda ()
                      (dotimes (r passes)
                        (dolist (ast ast-series)
                          (evaluate ast :cache cache)))))
                  passes))

             (measure-uncached (passes)
               "Run PASSES iterations of the series through `evaluate-node`.
                Returns the average CPU time per pass."
               (/ (measure-cpu-time
                    (lambda ()
                      (dotimes (r passes)
                        (dolist (ast ast-series)
                          (evaluate-node ast)))))
                  passes)))

      ;; --- Phase 1: Conventional baseline (no caching) ---
      (gc-and-settle)
      (let* ((conv-time (measure-uncached repeats)))

        ;; --- Phase 2: SMC cold pass ---
        (let ((cache (make-cache cache-max-size)))
          ;; For Level 1, disable compiled dispatch by setting compile-threshold
          ;; to most-positive-fixnum, so ensure-compiled-lookup never compiles.
          (when (eq level :l1)
            (setf (cache-compile-threshold cache) most-positive-fixnum))

          (gc-and-settle)
          (let* ((cold-time (measure-cached cache repeats)))

            ;; --- Phase 3: SMC warm pass (cache is already populated) ---
            (gc-and-settle)
            (let* ((warm-time (measure-cached cache repeats))
                   (post-warm-stats (cache-statistics cache)))

              (make-benchmark-trial
               :category name
               :count count
               :level level
               :seed seed
               :conventional-time conv-time
               :smc-cold-time cold-time
               :smc-warm-time warm-time
               :cache-stats post-warm-stats
               :unique-ratio unique-frac
               :conventional-gc-time 0.0
               :smc-cold-gc-time 0.0
               :smc-warm-gc-time 0.0))))))))

;;; ---------------------------------------------------------------------------
;;; Multi-trial runner with aggregation
;;; ---------------------------------------------------------------------------

(defun run-trials (name-generator-fn count &key (level :l1)
                                               (seeds *benchmark-default-seeds*)
                                               (cache-max-size 0))
  "Run independent trials across multiple seeds and return a TRIAL-AGGREGATE.
   NAME-GENERATOR-FN is a function of (count seed) that returns (values name ast-series).
   This structure matches how series-generators work: they're functions of (count &optional seed)."
  (let ((trials '()))
    (dolist (seed seeds)
      (multiple-value-bind (name series)
          (funcall name-generator-fn count seed)
        (push (run-single-trial name series
                                :count count
                                :level level
                                :seed seed
                                :cache-max-size cache-max-size)
              trials)))
    (aggregate-trials (nreverse trials))))

;;; ---------------------------------------------------------------------------
;;; End-to-end benchmark mode (parse + evaluate)
;;; ---------------------------------------------------------------------------

(defun run-single-trial-e2e (name expression-strings &key (count 1000) (level :l1) (seed 42)
                                                          (cache-max-size 0))
  "Run a single end-to-end trial: parse each string, then evaluate.
   This measures the full pipeline including parser overhead and parse-cache effects."
  (declare (ignore seed))
  (let* ((repeats (compute-repeats count))
         (asts (mapcar #'parse expression-strings))
         (unique-frac (unique-ratio asts)))

    (labels ((measure-cached (cache passes)
               (/ (measure-cpu-time
                    (lambda ()
                      (dotimes (r passes)
                        (dolist (str expression-strings)
                          (evaluate (parse str) :cache cache)))))
                  passes))

             (measure-uncached (passes)
               (/ (measure-cpu-time
                    (lambda ()
                      (dotimes (r passes)
                        (dolist (str expression-strings)
                          (evaluate-node (parse str))))))
                  passes)))

      (gc-and-settle)
      (let* ((conv-time (measure-uncached repeats)))

        (let ((cache (make-cache cache-max-size)))
          (when (eq level :l1)
            (setf (cache-compile-threshold cache) most-positive-fixnum))

          (gc-and-settle)
          (let* ((cold-time (measure-cached cache repeats)))

            (gc-and-settle)
            (let* ((warm-time (measure-cached cache repeats))
                   (post-warm-stats (cache-statistics cache)))

              (make-benchmark-trial
               :category name
               :count count
               :level level
               :seed seed
               :conventional-time conv-time
               :smc-cold-time cold-time
               :smc-warm-time warm-time
               :cache-stats post-warm-stats
               :unique-ratio unique-frac
               :conventional-gc-time 0.0
               :smc-cold-gc-time 0.0
               :smc-warm-gc-time 0.0))))))))

(defun run-trials-e2e (name-generator-fn count &key (level :l1)
                                                  (seeds *benchmark-default-seeds*)
                                                  (cache-max-size 0))
  "Run end-to-end trials across multiple seeds.
   NAME-GENERATOR-FN returns (values name string-list)."
  (let ((trials '()))
    (dolist (seed seeds)
      (multiple-value-bind (name strings)
          (funcall name-generator-fn count seed)
        (push (run-single-trial-e2e name strings
                                    :count count
                                    :level level
                                    :seed seed
                                    :cache-max-size cache-max-size)
              trials)))
    (aggregate-trials (nreverse trials))))

;;; ---------------------------------------------------------------------------
;;; Speedup formatting helper
;;; ---------------------------------------------------------------------------

(defun format-speedup-range (med min max)
  "Return a string like '1.23× [0.98, 1.45]' handling zero/INF cases."
  (flet ((fmt (val)
           (if (or (zerop val) (>= val most-positive-fixnum))
               "INF"
               (format nil "~2,2F" val))))
    (format nil "~A [~A,~A]" (fmt med) (fmt min) (fmt max))))

;;; ---------------------------------------------------------------------------
;;; Printing / Reporting
;;; ---------------------------------------------------------------------------

(defun print-trial-aggregate (agg)
  "Print a single aggregated trial result with median and range."
  (let* ((category (trial-aggregate-category agg))
         (count (trial-aggregate-count agg))
         (conv-med (trial-aggregate-conventional-time-median agg))
         (cold-med (trial-aggregate-smc-cold-time-median agg))
         (warm-med (trial-aggregate-smc-warm-time-median agg))
         (cold-sp-str (format-speedup-range
                       (trial-aggregate-cold-speedup-median agg)
                       (trial-aggregate-cold-speedup-min agg)
                       (trial-aggregate-cold-speedup-max agg)))
         (warm-sp-str (format-speedup-range
                       (trial-aggregate-warm-speedup-median agg)
                       (trial-aggregate-warm-speedup-min agg)
                       (trial-aggregate-warm-speedup-max agg)))
         (unique (trial-aggregate-unique-ratio agg))
         (stats (trial-aggregate-cache-stats agg))
         (ntrials (trial-aggregate-trial-count agg)))
    (format t "~%-- ~A (~D calculations, ~D trials) --~%" category count ntrials)
    (format t "  Unique ratio: ~,1F%%~%" (* unique 100))
    (format t "  Conventional: ~,6F s (median)~%" conv-med)
    (format t "  Cold pass:    ~,6F s (median) -- cold speedup: ~A~%" cold-med cold-sp-str)
    (format t "  Warm pass:    ~,6F s (median) -- warm speedup: ~A~%" warm-med warm-sp-str)
    (format t "  Cache stats: size=~D hits=~D misses=~D (median trial)~%"
            (getf stats :size) (getf stats :hits) (getf stats :misses))
    (let ((conv-gc (trial-aggregate-conventional-gc-median agg))
          (cold-gc (trial-aggregate-smc-cold-gc-median agg))
          (warm-gc (trial-aggregate-smc-warm-gc-median agg)))
      (format t "  GC time: conventional=~,4F s  cold=~,4F s  warm=~,4F s~%"
              conv-gc cold-gc warm-gc))))

(defun print-aggregate-summary-table (aggregates)
  "Print a summary table of aggregated results.
   AGGREGATES is a list of TRIAL-AGGREGATE structs at the same level."
  (let* ((first-agg (first aggregates))
         (level (trial-aggregate-level first-agg))
         (ntrials (trial-aggregate-trial-count first-agg)))
    (format t "~%~%=== Benchmark Summary (level ~A, ~D trials) ===~%" level ntrials)
    (format t "~A~%" (make-string 120 :initial-element #\-))
    (format t "~25A ~6A ~7A ~10A ~10A ~10A ~22A~%"
            "Category" "Count" "Unique%" "Conv(ms)" "Cold(ms)" "Warm(ms)" "Warm Speedup")
    (format t "~A~%" (make-string 120 :initial-element #\-))
    (dolist (agg (sort (copy-list aggregates) #'string< :key #'trial-aggregate-category))
      (let* ((count (trial-aggregate-count agg))
             (conv-ms (* 1000.0 (/ (trial-aggregate-conventional-time-median agg) count)))
             (cold-ms (* 1000.0 (/ (trial-aggregate-smc-cold-time-median agg) count)))
             (warm-ms (* 1000.0 (/ (trial-aggregate-smc-warm-time-median agg) count)))
             (warm-sp-str (format-speedup-range
                           (trial-aggregate-warm-speedup-median agg)
                           (trial-aggregate-warm-speedup-min agg)
                           (trial-aggregate-warm-speedup-max agg)))
             (unique-pct (* 100 (trial-aggregate-unique-ratio agg))))
        (format t "~25A ~6D ~6,1F ~10,6F ~10,6F ~10,6F ~22A~%"
                (trial-aggregate-category agg) count unique-pct
                conv-ms cold-ms warm-ms warm-sp-str)))))

(defun print-level-comparison (all-aggregates)
  "Print a comparison table across optimization levels.
   ALL-AGGREGATES is a list of TRIAL-AGGREGATE structs at various levels.
   Assumes all aggregates at the same (category, count) share the same size."
  (let* ((levels (remove-duplicates (mapcar #'trial-aggregate-level all-aggregates)))
         (categories (remove-duplicates (mapcar #'trial-aggregate-category all-aggregates)
                                        :test #'string=))
         ;; Group by (category, count) -> alist (level . warm-speedup-median)
         (grouped (make-hash-table :test 'equal)))
    ;; Populate lookup: key = (category . count) -> alist (level . warm-speedup-median)
    (dolist (agg all-aggregates)
      (let* ((cat (trial-aggregate-category agg))
             (cnt (trial-aggregate-count agg))
             (key (cons cat cnt))
             (lev (trial-aggregate-level agg))
             (sp (trial-aggregate-warm-speedup-median agg)))
        (push (cons lev sp) (gethash key grouped))))
    ;; Print for each size
    (let ((sizes (sort (remove-duplicates (mapcar #'trial-aggregate-count all-aggregates)) #'<)))
      (dolist (size sizes)
        (format t "~%~%=== Optimization Level Comparison (~D calculations) ===~%" size)
        (format t "~A~%" (make-string 80 :initial-element #\-))
        (format t "~30A" "Category")
        (dolist (lev levels)
          (format t " ~12A" (string-upcase lev)))
        (format t "~%")
        (format t "~A~%" (make-string 80 :initial-element #\-))
        (dolist (cat (sort (copy-list categories) #'string<))
          (let ((entry (gethash (cons cat size) grouped)))
            (when entry
              (format t "~30A" cat)
              (dolist (lev levels)
                (let ((speedup (or (cdr (assoc lev entry)) 0.0)))
                  (if (or (zerop speedup) (>= speedup most-positive-fixnum))
                      (format t " ~12A" "INF")
                      (format t " ~11,2F×" speedup))))
              (format t "~%"))))))))

;;; ---------------------------------------------------------------------------
;;; Machine-readable sexp output
;;; ---------------------------------------------------------------------------

(defun print-aggregates-as-sexp (aggregates)
  "Print a Lisp-readable sexp summarizing all TRIAL-AGGREGATE structs.
   The output is a plist with keys :category, :count, :level, :unique-ratio,
   :warm-speedup-median, :warm-speedup-min, :warm-speedup-max, etc.
   This is read by analysis tools and the run-benchmarks.sh script."
  (let ((*print-readably* t)
        (*print-pretty* t))
    (format t "~%(benchmark-results~%")
    (dolist (agg aggregates)
      (format t "  (~S~%" (trial-aggregate-category agg))
      (format t "   :count ~D~%" (trial-aggregate-count agg))
      (format t "   :level ~S~%" (trial-aggregate-level agg))
      (format t "   :trials ~D~%" (trial-aggregate-trial-count agg))
      (format t "   :unique-ratio ~,4F~%" (trial-aggregate-unique-ratio agg))
      (format t "   :conv-time-median ~,8F~%" (trial-aggregate-conventional-time-median agg))
      (format t "   :warm-time-median ~,8F~%" (trial-aggregate-smc-warm-time-median agg))
      (format t "   :cold-time-median ~,8F~%" (trial-aggregate-smc-cold-time-median agg))
      (format t "   :warm-speedup-median ~,4F~%" (trial-aggregate-warm-speedup-median agg))
      (format t "   :warm-speedup-min ~,4F~%" (trial-aggregate-warm-speedup-min agg))
      (format t "   :warm-speedup-max ~,4F~%" (trial-aggregate-warm-speedup-max agg))
      (format t "   :cold-speedup-median ~,4F~%" (trial-aggregate-cold-speedup-median agg))
      (format t "   :cache-size ~D~%" (getf (trial-aggregate-cache-stats agg) :size))
      (format t "   :cache-hits ~D~%" (getf (trial-aggregate-cache-stats agg) :hits))
      (format t "   :cache-misses ~D~%" (getf (trial-aggregate-cache-stats agg) :misses))
      (format t "   :conv-gc-median ~,4F~%" (trial-aggregate-conventional-gc-median agg))
      (format t "   :cold-gc-median ~,4F~%" (trial-aggregate-smc-cold-gc-median agg))
      (format t "   :warm-gc-median ~,4F~%" (trial-aggregate-smc-warm-gc-median agg))
      (format t ")~%"))
    (format t ")~%")))

;;; ---------------------------------------------------------------------------
;;; Configuration helpers for optimization levels
;;; ---------------------------------------------------------------------------

(defun ensure-base-operator-table ()
  "Reset the operator table to its original, un-specialized functions.
   This undoes any Level 2 specialization that may have been installed."
  (maphash (lambda (op base-fn)
             (setf (gethash op *operator-table*) base-fn))
           *original-operator-functions*))

(defun enable-arithmetic-specialization (&optional (threshold 5))
  "Enable Level 2 operator specialization for arithmetic operators used in benchmarks."
  (dolist (op '(:+ :* :- :/))
    (enable-operator-specialization op threshold)))

(defun setup-warmup-for-level-2 (series)
  "Run a warmup pass with specialization enabled.
   This triggers operator specialization by running SERIES once,
   allowing call-count thresholds to be crossed.
   After this, *operator-table* contains the steady-state specialized functions.
   Returns the cache populated during warmup (discarded afterward)."
  (ensure-base-operator-table)
  (enable-arithmetic-specialization)
  (let ((cache (make-cache)))
    (dolist (ast series)
      (evaluate ast :cache cache))
    (values cache)))