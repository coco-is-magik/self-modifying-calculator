;;;; dispatch-compiler.lisp
;;;; Compile a cache into a fast dispatch function for whole-expression hits.
;;;;
;;;; The generated function takes one AST node and returns two values:
;;;;   (values cached-value :found)    if the expression is in the cache
;;;;   (values nil nil)                 otherwise
;;;;
;;;; Sub-expression lookup still falls back to the EQ hash table.

(in-package :self-modifying-calculator)

(defun compile-cache-dispatch (cache)
  "Generate and compile a dispatch function for the whole-expression entries
   in CACHE. Returns the compiled function. Uses EQ clauses because AST keys
   are canonical interned objects."
  (let ((clauses '()))
    (maphash (lambda (key value)
               (push `((eq ',key node) (values ,value :found)) clauses))
             (cache-table cache))
    (let ((dispatch-form
            `(lambda (node)
               (declare (optimize (speed 3) (safety 1)))
               (cond
                 ,@(nreverse clauses)
                 (t (values nil nil))))))
      (compile nil dispatch-form))))

(defun ensure-compiled-lookup (cache)
  "Return the cache's compiled lookup function, recompiling if necessary."
  (when (cache-needs-recompile-p cache)
    (setf (cache-compiled-lookup cache) (compile-cache-dispatch cache))
    (mark-cache-compiled cache))
  (cache-compiled-lookup cache))

(defun compiled-cache-get (cache node)
  "Use the compiled dispatch to lookup NODE in CACHE.
   Returns (values value found-p)."
  (let ((fn (ensure-compiled-lookup cache)))
    (if fn
        (funcall fn node)
        (values nil nil))))
