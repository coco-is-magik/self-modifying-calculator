;;;; benchmark-framework.lisp
;;;; Common timing, reporting, and execution utilities for the benchmark suite.

(in-package :self-modifying-calculator)

(defstruct benchmark-result
  "Result of a single benchmark run."
  category
  count
  conventional-time
  smc-cold-time
  smc-warm-time
  cache-stats)

(defun measure-time (thunk)
  "Measure the wall-clock time taken by THUNK. Returns seconds as a float."
  (let ((start (get-internal-real-time)))
    (funcall thunk)
    (let ((end (get-internal-real-time)))
      (/ (- end start) internal-time-units-per-second))))

(defun print-benchmark-result (result)
  "Print a single benchmark result in a readable format."
  (let ((category (benchmark-result-category result))
        (count (benchmark-result-count result))
        (conv (benchmark-result-conventional-time result))
        (cold (benchmark-result-smc-cold-time result))
        (warm (benchmark-result-smc-warm-time result))
        (stats (benchmark-result-cache-stats result)))
    (format t "~%-- ~A (~D calculations) --~%" category count)
    (format t "Conventional: ~,4F s (~,9F ms/calc)~%"
            conv (* 1000.0 (if (zerop count) 0 (/ conv count))))
    (format t "SMC cold:     ~,4F s (~,9F ms/calc)~%"
            cold (* 1000.0 (if (zerop count) 0 (/ cold count))))
    (format t "SMC warm:     ~,4F s (~,9F ms/calc) -- speedup: ~,2Fx~%"
            warm (* 1000.0 (if (zerop count) 0 (/ warm count)))
            (if (zerop warm) :inf (/ conv warm)))
    (format t "Cache stats: size=~D hits=~D misses=~D~%"
            (getf stats :size)
            (getf stats :hits)
            (getf stats :misses))))

(defun run-benchmark-category (name ast-series &key (count 1000))
  "Run the conventional and SMC benchmark for a given AST series.
   AST-SERIES is a list of AST nodes (not strings). Returns a benchmark-result.
   For short/medium series, each pass is repeated enough times to accumulate
   measurable wall-clock time."
  (let* ((cache (make-cache))
         (repeats (cond ((<= count 100) 100)
                        ((<= count 10000) 10)
                        (t 1)))
         (conv-time
           (/ (measure-time (lambda ()
                             (dotimes (r repeats)
                               (dolist (ast ast-series)
                                 (evaluate-node ast)))))
              repeats))
         (cold-time
           (/ (measure-time (lambda ()
                             (dotimes (r repeats)
                               (dolist (ast ast-series)
                                 (evaluate ast :cache cache)))))
              repeats))
         (warm-time
           (/ (measure-time (lambda ()
                             (dotimes (r repeats)
                               (dolist (ast ast-series)
                                 (evaluate ast :cache cache)))))
              repeats)))
    (make-benchmark-result
     :category name
     :count count
     :conventional-time conv-time
     :smc-cold-time cold-time
     :smc-warm-time warm-time
     :cache-stats (cache-statistics cache))))

(defun print-summary-table (results)
  "Print a summary table of all benchmark results."
  (format t "~%=== Benchmark Summary ===~%")
  (format t "~A~%" (make-string 75 :initial-element #\-))
  (format t "~25A ~10A ~10A ~10A ~10A~%" "Category" "Count" "Conv(ms)" "Warm(ms)" "Speedup")
  (format t "~A~%" (make-string 75 :initial-element #\-))
  (dolist (r results)
    (let* ((count (benchmark-result-count r))
           (conv-ms (* 1000.0 (/ (benchmark-result-conventional-time r) count)))
           (warm-ms (* 1000.0 (/ (benchmark-result-smc-warm-time r) count)))
           (speedup (if (zerop (benchmark-result-smc-warm-time r))
                        :inf
                        (/ (benchmark-result-conventional-time r)
                           (benchmark-result-smc-warm-time r)))))
      (format t "~25A ~10D ~10,6F ~10,6F ~10,2F~%"
              (benchmark-result-category r)
              count
              conv-ms
              warm-ms
              speedup))))
