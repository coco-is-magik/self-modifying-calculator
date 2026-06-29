;;;; dispatch-compiler.lisp
;;;; Compile a cache into a fast dispatch function for whole-expression hits.
;;;;
;;;; The generated function takes one AST node and returns two values:
;;;;   (values cached-value :found)    if the expression is in the cache
;;;;   (values nil nil)                 otherwise
;;;;
;;;; Sub-expression lookup still falls back to the EQ hash table.
;;;;
;;;; The compiled dispatch is limited to caches with <= 100 entries to avoid the
;;;; stack overflow and slow linear scan caused by thousands of cond clauses.

(in-package :self-modifying-calculator)

(defparameter *compiled-dispatch-max-clauses* 100
  "Maximum number of clauses to include in a compiled dispatch function.")

(defun compile-cache-dispatch (cache)
  "Generate and compile a dispatch function for the entries in CACHE.
   Returns the compiled function or nil if the cache is too large."
  (let ((entries '()))
    (maphash (lambda (key value)
               (push (cons key value) entries))
             (cache-table cache))
    (when (<= (length entries) *compiled-dispatch-max-clauses*)
      (let* ((clauses (mapcar (lambda (entry)
                                `((eq ',(car entry) node) (values ',(cdr entry) :found)))
                              entries))
             (dispatch-form `(lambda (node)
                               (declare (optimize (speed 3) (safety 1)))
                               (cond ,@clauses (t (values nil nil))))))
        (compile nil dispatch-form)))))

(defun ensure-compiled-lookup (cache)
  "Return the cache's compiled lookup function, recompiling if necessary.
   If the cache is too large, disable the compiled lookup and return nil."
  (when (cache-needs-recompile-p cache)
    (let ((fn (compile-cache-dispatch cache)))
      (setf (cache-compiled-lookup cache) fn)
      (mark-cache-compiled cache)
      (unless fn
        ;; Cache has grown beyond the compiled limit; stop trying to recompile.
        (setf (cache-compile-threshold cache) most-positive-fixnum))))
  (cache-compiled-lookup cache))

(defun compiled-cache-get (cache node)
  "Use the compiled dispatch to lookup NODE in CACHE.
   Returns (values value found-p)."
  (let ((fn (ensure-compiled-lookup cache)))
    (if fn
        (funcall fn node)
        (values nil nil))))
