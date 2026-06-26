;;;; dot-product-benchmark.lisp
;;;; Benchmark comparing conventional vs cache-aware dot product evaluation.
;;;; This is a realistic workload for the target use case (renderers, 3D games).

(in-package :self-modifying-calculator)

(defun generate-dot-product-series (count)
  "Generate COUNT dot-product expression strings with a fixed light direction
   and randomized surface normals. Reuses Nz*Lz repeatedly."
  (let ((rng (make-random-state t)))
    (loop repeat count
          collect (let ((nx (1- (random 3 rng)))  ; -1, 0, or 1
                        (ny (1- (random 3 rng)))
                        (nz (1- (random 3 rng)))
                        (lx 0.6)
                        (ly 0.0)
                        (lz 0.8))
                    (format nil "(~D*~F)+(~D*~F)+(~D*~F)"
                            nx lx ny ly nz lz)))))

(defun run-dot-product-benchmark (&key (count 100000))
  "Run a dot-product benchmark and report speedups."
  (format t "=== Dot Product Benchmark (~D calculations) ===~%" count)
  (let ((series (generate-dot-product-series count))
        (cache (make-cache)))

    (let ((start (get-internal-real-time)))
      (dolist (expr series)
        (evaluate-node (parse expr)))
      (let ((conv-time (/ (- (get-internal-real-time) start) internal-time-units-per-second)))
        (format t "Conventional: ~,4F seconds (~,9F ms/calc)~%" conv-time (* 1000.0 (/ conv-time count)))

        (let ((start2 (get-internal-real-time)))
          (dolist (expr series)
            (evaluate (parse expr) :cache cache))
          (let ((cold-time (/ (- (get-internal-real-time) start2) internal-time-units-per-second)))
            (format t "SMC cold:     ~,4F seconds (~,9F ms/calc)~%" cold-time (* 1000.0 (/ cold-time count)))

            (let ((start3 (get-internal-real-time)))
              (dolist (expr series)
                (evaluate (parse expr) :cache cache))
              (let ((warm-time (/ (- (get-internal-real-time) start3) internal-time-units-per-second)))
                (format t "SMC warm:     ~,4F seconds (~,9F ms/calc) -- speedup vs conventional: ~,2Fx~%"
                        warm-time (* 1000.0 (/ warm-time count))
                        (if (zerop warm-time) :inf (/ conv-time warm-time)))
                (let ((stats (cache-statistics cache)))
                  (format t "Cache stats: size=~D hits=~D misses=~D~%"
                          (getf stats :size)
                          (getf stats :hits)
                          (getf stats :misses)))))))))))
