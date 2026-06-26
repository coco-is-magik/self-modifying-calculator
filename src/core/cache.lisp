;;;; cache.lisp
;;;; In-memory cache for expression and sub-expression results.
;;;;
;;;; Cache keys are AST nodes (lists). Because AST nodes are structural,
;;;; the cache uses EQUAL for matching. Only ground expressions (no variables)
;;;; are cached.

(in-package :self-modifying-calculator)

(defstruct (cache (:constructor %make-cache))
  "Cache structure holding a hash table of AST -> value mappings."
  (table (make-hash-table :test 'equal) :type hash-table)
  (hits 0 :type integer)
  (misses 0 :type integer))

(defun make-cache ()
  "Create a fresh, empty cache."
  (%make-cache))

(defun cache-get (cache key)
  "Lookup KEY in CACHE. Returns (values value found-p)."
  (let ((table (cache-table cache)))
    (multiple-value-bind (value found) (gethash key table)
      (values value found))))

(defun cache-set (cache key value)
  "Store VALUE for KEY in CACHE. Only ground expressions are cached."
  (when (ast-ground-p key)
    (setf (gethash key (cache-table cache)) value)))

(defun cache-contains-p (cache key)
  "Return true if CACHE contains KEY."
  (multiple-value-bind (value found) (cache-get cache key)
    (declare (ignore value))
    found))

(defun cache-clear (cache)
  "Remove all entries from CACHE."
  (clrhash (cache-table cache))
  (setf (cache-hits cache) 0)
  (setf (cache-misses cache) 0)
  nil)

(defun cache-size (cache)
  "Return the number of entries in CACHE."
  (hash-table-count (cache-table cache)))

(defun cache-statistics (cache)
  "Return a plist of cache statistics."
  (list :size (cache-size cache)
        :hits (cache-hits cache)
        :misses (cache-misses cache)))

(defun ast-ground-p (node)
  "Return true if NODE contains no variable references."
  (let ((ground t))
    (walk-ast node
              (lambda (n)
                (when (variable-node-p n)
                  (setf ground nil))))
    ground))

(defparameter *global-cache* (make-cache)
  "Default global cache used by the cache-aware evaluator.")
