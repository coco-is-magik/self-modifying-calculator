;;;; optimizer.lisp
;;;; Level 2 self-modification: runtime function specialization.
;;;;
;;;; The optimizer watches arithmetic operator calls with specific constant
;;;; operands. Once a pattern is seen a threshold number of times, it generates
;;;; a specialized function that short-circuits those operand pairs and
;;;; hot-swaps it into the operator registry via fdefinition.

(in-package :self-modifying-calculator)

;;; Specialization tracking

(defstruct specialization-state
  "Tracks how often each operator is called with specific constant operands."
  (threshold 5 :type integer)
  (counts (make-hash-table :test 'equal) :type hash-table))

(defparameter *operator-specializations* (make-hash-table :test 'eq)
  "Map from operator symbol to its specialization-state.")

(defun get-specialization-state (op)
  (or (gethash op *operator-specializations*)
      (setf (gethash op *operator-specializations*) (make-specialization-state))))

(defun specialization-key (args)
  "Create a key for a specific operand list if all args are numbers, else nil."
  (when (every #'numberp args)
    args))

(defun record-operator-call (op args)
  "Record a call to operator OP with ARGS. Returns the updated call count."
  (let* ((state (get-specialization-state op))
         (key (specialization-key args)))
    (when key
      (let* ((counts (specialization-state-counts state))
             (new-count (1+ (gethash key counts 0))))
        (setf (gethash key counts) new-count)
        new-count))))

(defun specialization-patterns (op threshold)
  "Return all operand patterns for OP that have been seen at least THRESHOLD times."
  (let ((state (get-specialization-state op))
        (result '()))
    (maphash (lambda (key count)
               (when (>= count threshold)
                 (push key result)))
             (specialization-state-counts state))
    (nreverse result)))

;;; Specialized function generation

(defun generate-specialized-function (op base-fn patterns)
  "Generate a compiled function that short-circuits PATTERNS and falls back to BASE-FN."
  (declare (ignore op))
  (let ((arity (length (first patterns))))
    (let ((args (loop for i below arity collect (gensym "ARG"))))
      (compile nil
               `(lambda ,args
                  (cond
                    ,@(mapcar (lambda (pattern)
                                `((and ,@(mapcar (lambda (arg value) `(= ,arg ,value)) args pattern))
                                  ,(apply base-fn pattern)))
                              patterns)
                    (t (apply ,base-fn (list ,@args)))))))))

(defun specialize-operator (op &optional (threshold 5))
  "If OP has operand patterns seen at least THRESHOLD times, install a specialized function."
  (let ((patterns (specialization-patterns op threshold)))
    (when (and patterns (operator-base-function op))
      (let* ((base-fn (operator-base-function op))
             (specialized (generate-specialized-function op base-fn patterns)))
        ;; Install the specialized function and reset specialization state.
        (setf (gethash op *operator-table*) specialized)
        (setf (gethash op *operator-specializations*) (make-specialization-state :threshold threshold))
        (format *trace-output* "Specialized operator ~A for ~D pattern(s).~%" op (length patterns))
        t))))

(defun wrap-operator-with-specialization (op threshold)
  "Create a wrapper that records calls and periodically triggers specialization.
   Always delegates to the original base function, never to the current operator table entry."
  (let ((base-fn (operator-base-function op)))
    (lambda (&rest args)
      (let ((count (record-operator-call op args)))
        (when (and count (>= count threshold))
          (specialize-operator op threshold))
        (apply base-fn args)))))

(defun install-specializing-wrapper (op &optional (threshold 5))
  "Wrap OP's current function so that future calls are tracked for specialization."
  (setf (gethash op *operator-table*)
        (wrap-operator-with-specialization op threshold))
  (setf (gethash op *operator-specializations*)
        (make-specialization-state :threshold threshold))
  t)

(defun enable-operator-specialization (op &optional (threshold 5))
  "Enable specialization tracking for operator OP."
  (install-specializing-wrapper op threshold))
