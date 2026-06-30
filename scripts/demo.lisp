;;;; scripts/demo.lisp
;;;; A short, self-contained demo of the self-modifying calculator.
;;;; Run it with:
;;;;   ./scripts/run-demo.sh
;;;;
;;;; Or manually:
;;;;   sbcl --noinform \
;;;;        --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
;;;;        --eval "(asdf:load-system :self-modifying-calculator)" \
;;;;        --eval "(load \"scripts/demo.lisp\")" \
;;;;        --eval "(sb-ext:exit)"

(in-package :self-modifying-calculator)

;; Load the benchmark framework components.
(load (merge-pathnames #p"tests/benchmarks/benchmark-framework.lisp" *default-pathname-defaults*))
(load (merge-pathnames #p"tests/benchmarks/series-generators.lisp" *default-pathname-defaults*))

(defun run-demo (&optional (count 10000))
  "Run a focused realistic-renderer benchmark and print a narrative summary."
  (format t "~%Self-Modifying Calculator Demo~%")
  (format t "==================================~%")
  (format t "Running ~D realistic renderer-style calculations.~%" count)
  (format t "These are dot(N,L) * sin(angle) + 0.1 style expressions.~%~%")

  (let* ((series (generate-realistic-renderer-asts count))
         (result (run-benchmark-category "Realistic Renderer" series :count count)))
    (format t "Conventional time: ~,4F s~%" (benchmark-result-conventional-time result))
    (format t "SMC warm time:     ~,4F s~%" (benchmark-result-smc-warm-time result))
    (format t "Speedup:           ~,2F×~%"
            (/ (benchmark-result-conventional-time result)
               (benchmark-result-smc-warm-time result)))
    (let ((stats (benchmark-result-cache-stats result)))
      (format t "Cache:             ~D entries, ~D hits, ~D misses~%"
              (getf stats :size)
              (getf stats :hits)
              (getf stats :misses))))
  (format t "~%Try the full benchmark suite with:~%")
  (format t "  (load \"tests/benchmarks/run-all-benchmarks.lisp\")~%")
  (format t "  (smc::run-all-benchmarks)~%"))

(run-demo)
