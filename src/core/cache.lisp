;;;; cache.lisp
;;;; In-memory cache for expression and sub-expression results.

(in-package :self-modifying-calculator)

(defstruct (cache (:constructor %make-cache))
  "Cache structure holding a hash table of AST -> value mappings."
  (table (make-hash-table :test 'eq) :type hash-table)
  (hits 0 :type integer)
  (misses 0 :type integer)
  (compiled-lookup nil)         ; compiled (lambda (node) ...) or nil
  (compiled-lookup-dirty t)     ; t when table has changed since last compile
  (compile-threshold 20)        ; only compile when cache has >= this many entries
  (last-compiled-size 0)       ; cache size at last compile; recompile when doubled
  (max-size 0 :type integer)      ; 0 means unlimited
  (access-counter 0 :type integer)  ; monotonic access counter for LRU eviction
  (last-access (make-hash-table :test 'eq) :type hash-table))  ; key -> access count

(defun make-cache (&optional (max-size 0))
  "Create a fresh, empty cache. If MAX-SIZE is positive, the cache will evict
   the least recently used entry when the size exceeds MAX-SIZE."
  (let ((cache (%make-cache :max-size max-size)))
    (setf (cache-last-access cache) (make-hash-table :test 'eq))
    cache))

(defun cache-touch (cache key)
  "Record that KEY was accessed in CACHE."
  (incf (cache-access-counter cache))
  (setf (gethash key (cache-last-access cache)) (cache-access-counter cache)))

(defun cache-evict-lru (cache)
  "Evict the least recently used entry from CACHE. Returns the evicted key."
  (let ((table (cache-table cache))
        (lru-table (cache-last-access cache))
        (oldest-key nil)
        (oldest-time nil))
    (maphash (lambda (key value)
               (declare (ignore value))
               (let ((last-access (gethash key lru-table 0)))
                 (when (or (null oldest-time) (< last-access oldest-time))
                   (setf oldest-time last-access)
                   (setf oldest-key key))))
             table)
    (when oldest-key
      (remhash oldest-key table)
      (remhash oldest-key lru-table)
      (setf (cache-compiled-lookup-dirty cache) t)
      oldest-key)))

(defun cache-get (cache key)
  "Lookup KEY in CACHE. Returns (values value found-p)."
  (let ((table (cache-table cache)))
    (multiple-value-bind (value found) (gethash key table)
      (when found
        (cache-touch cache key))
      (values value found))))

(defun cache-set (cache key value)
  "Store VALUE for KEY in CACHE. Only ground expressions are cached.
   KEY is interned to ensure EQ-based cache lookups work correctly.
   If the cache has a positive MAX-SIZE and is full, evict the LRU entry."
  (let ((key (intern-ast key)))
    (when (ast-ground-p key)
      (let ((max-size (cache-max-size cache)))
        (when (and (> max-size 0)
                   (>= (cache-size cache) max-size)
                   (not (cache-contains-p cache key)))
          (cache-evict-lru cache))
        (setf (gethash key (cache-table cache)) value)
        (cache-touch cache key)
        (setf (cache-compiled-lookup-dirty cache) t)
        value))))

(defun cache-contains-p (cache key)
  "Return true if CACHE contains KEY."
  (multiple-value-bind (value found) (cache-get cache key)
    (declare (ignore value))
    found))

(defun cache-clear (cache)
  "Remove all entries from CACHE."
  (clrhash (cache-table cache))
  (clrhash (cache-last-access cache))
  (setf (cache-hits cache) 0)
  (setf (cache-misses cache) 0)
  (setf (cache-access-counter cache) 0)
  (setf (cache-compiled-lookup-dirty cache) t)
  nil)

(defun cache-needs-recompile-p (cache)
  "Return true if the cache's compiled lookup is stale and should be regenerated.
   Recompile when the cache first reaches the threshold, or when it has doubled
   in size since the last compilation."
  (let ((size (cache-size cache))
        (threshold (cache-compile-threshold cache))
        (last-size (cache-last-compiled-size cache)))
    (and (>= size threshold)
         (cache-compiled-lookup-dirty cache)
         (or (zerop last-size)
             (>= size (* 2 last-size))))))

(defun mark-cache-compiled (cache)
  "Record that the cache has just been compiled at its current size."
  (setf (cache-compiled-lookup-dirty cache) nil)
  (setf (cache-last-compiled-size cache) (cache-size cache)))

(defun cache-size (cache)
  "Return the number of entries in CACHE."
  (hash-table-count (cache-table cache)))

(defun cache-statistics (cache)
  "Return a plist of cache statistics."
  (list :size (cache-size cache)
        :hits (cache-hits cache)
        :misses (cache-misses cache)
        :max-size (cache-max-size cache)))

;;; Persistence

(defparameter *cache-file-path*
  (merge-pathnames #p"cache/cache.sexp" *default-pathname-defaults*)
  "Default path for the serialized cache file.")

(defun save-cache (cache &optional (path *cache-file-path*))
  "Serialize CACHE to PATH. Only ground entries are saved."
  (ensure-directories-exist path)
  (with-open-file (stream path :direction :output :if-exists :supersede :if-does-not-exist :create)
    (let ((*print-readably* t)
          (*print-pretty* t))
      (format stream ";;;; Self-Modifying Calculator cache~%")
      (format stream ";;;; Generated automatically. Do not hand-edit unless you know what you are doing.~%")
      (format stream "(~%")
      (maphash (lambda (key value)
                 (format stream "  (~S . ~S)~%" key value))
               (cache-table cache))
      (format stream ")~%")))
  path)

(defun load-cache (cache &optional (path *cache-file-path*))
  "Load entries from PATH into CACHE. Returns the number of entries loaded."
  (when (probe-file path)
    (with-open-file (stream path :direction :input)
      (let ((entries (read stream nil nil)))
        (when (listp entries)
          (dolist (entry entries)
            (when (consp entry)
              (cache-set cache (car entry) (cdr entry))))
          (length entries)))))
  0)

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

(defparameter *auto-persist-cache* nil
  "When true, `*global-cache*' is saved on process exit and loaded on startup.")

(defun maybe-load-global-cache ()
  "Load `*global-cache*' from `*cache-file-path*' if auto-persistence is enabled."
  (when *auto-persist-cache*
    (load-cache *global-cache* *cache-file-path*)))

(defun maybe-save-global-cache ()
  "Save `*global-cache*' to `*cache-file-path*' if auto-persistence is enabled."
  (when *auto-persist-cache*
    (save-cache *global-cache* *cache-file-path*)))
