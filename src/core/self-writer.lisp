;;;; self-writer.lisp
;;;; Level 3 self-modification: write accumulated cache entries back into a
;;;; Lisp source file as literal constants. Loading that file on startup
;;;; populates the cache without a warm-up period.

(in-package :self-modifying-calculator)

(defparameter *generated-cache-file*
  (merge-pathnames #p"src/generated/cache-literals.lisp" *default-pathname-defaults*)
  "Path to the generated cache source file.")

(defun write-cache-as-source (cache &optional (path *generated-cache-file*))
  "Write CACHE entries as a Lisp source file that populates the global cache
   when loaded. Returns the path written."
  (ensure-directories-exist path)
  (with-open-file (stream path :direction :output :if-exists :supersede :if-does-not-exist :create)
    (let ((*print-readably* t)
          (*print-pretty* t))
      (format stream ";;;; Generated cache literals for Self-Modifying Calculator~%")
      (format stream ";;;; This file is generated automatically. Do not hand-edit.~%")
      (format stream "(in-package :self-modifying-calculator)~%~%")
      (format stream "(let ((cache *global-cache*))~%")
      (maphash (lambda (key value)
                 (format stream "  (cache-set cache '~S ~S)~%" key value))
               (cache-table cache))
      (format stream "  (cache-size cache))~%")))
  path)

(defun load-generated-cache-source (&optional (path *generated-cache-file*))
  "If PATH exists, load it and return the number of cache entries loaded."
  (when (probe-file path)
    (let ((size-before (cache-size *global-cache*)))
      (load path)
      (- (cache-size *global-cache*) size-before))))
