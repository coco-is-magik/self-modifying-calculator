;;;; run-all-benchmarks.lisp
;;;; Entry point that runs all per-category benchmarks and prints a summary.

(in-package :self-modifying-calculator)

(defun enable-arithmetic-specialization (&optional (threshold 5))
  "Enable Level 2 operator specialization for arithmetic operators used in benchmarks."
  (dolist (op '(:+ :* :- :/))
    (enable-operator-specialization op threshold)))

(defun run-all-benchmarks (&key (short 100) (medium 10000) (long 100000) (enable-specialization t))
  "Run all benchmark categories at short, medium, and long lengths.
   Returns a list of all benchmark-result structs."
  (when enable-specialization
    (enable-arithmetic-specialization))
  (let ((results '())
        (sizes (list (cons :short short) (cons :medium medium) (cons :long long))))
    (dolist (size sizes)
      (let ((count (cdr size))
            (suffix (format nil " (~A)" (car size))))
        (push (run-benchmark-category
               (format nil "Arithmetic~A" suffix)
               (generate-arithmetic-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Dot Product~A" suffix)
               (generate-dot-product-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Cross Product~A" suffix)
               (generate-cross-product-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Trig~A" suffix)
               (generate-trig-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Polynomial~A" suffix)
               (generate-polynomial-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Mixed~A" suffix)
               (generate-mixed-asts count) :count count)
              results)
        (push (run-benchmark-category
               (format nil "Realistic Renderer~A" suffix)
               (generate-realistic-renderer-asts count) :count count)
              results)))
    (dolist (r (reverse results))
      (print-benchmark-result r))
    (print-summary-table (reverse results))
    (reverse results)))
