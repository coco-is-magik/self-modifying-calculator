;;;; pipeline.lisp
;;;; Unified 3-level optimization configuration.
;;;;
;;;; Levels:
;;;;   1 - Runtime EQ hash cache (always active via `evaluate-node-cached`).
;;;;   2 - Runtime operator specialization (installs call-recording wrappers).
;;;;   3 - Source-level cache rewriting (writes generated cache source file).

(in-package :self-modifying-calculator)

(defparameter *pipeline-level* 1
  "Highest optimization level currently enabled. 1 = cache only, 2 = +specialization, 3 = +source rewrite.")

(defparameter *pipeline-specialization-threshold* 5
  "Call count threshold for Level 2 operator specialization.")

(defparameter *pipeline-source-rewrite-threshold* 50
  "Cache size threshold for Level 3 source rewriting.")

(defparameter *pipeline-source-rewrite-interval* 100
  "Number of cache-set calls between Level 3 source rewrite attempts.")

(defvar *pipeline-set-count* 0
  "Number of cache-set calls since the last Level 3 rewrite.")

(defun pipeline-specialization-enabled-p ()
  "Return true if Level 2 operator specialization is active."
  (>= *pipeline-level* 2))

(defun pipeline-source-rewrite-enabled-p ()
  "Return true if Level 3 source rewriting is active."
  (>= *pipeline-level* 3))

(defun pipeline-enable-specialization ()
  "Enable Level 2 by wrapping arithmetic operators for specialization tracking."
  (dolist (op '(:+ :* :- :/))
    (when (operator-base-function op)
      (enable-operator-specialization op *pipeline-specialization-threshold*))))

(defun pipeline-rewrite-source ()
  "Perform Level 3 source rewriting if the global cache is large enough."
  (when (>= (cache-size *global-cache*) *pipeline-source-rewrite-threshold*)
    (write-cache-as-source *global-cache*)
    (setf *pipeline-set-count* 0)))

(defun pipeline-cache-set-hook (cache key value)
  "Hook called after each cache-set. Triggers Level 2 specialization and Level 3 source rewriting."
  (declare (ignore cache key value))
  (when (pipeline-specialization-enabled-p)
    (dolist (op '(:+ :* :- :/))
      (specialize-operator op *pipeline-specialization-threshold*)))
  (when (pipeline-source-rewrite-enabled-p)
    (incf *pipeline-set-count*)
    (when (>= *pipeline-set-count* *pipeline-source-rewrite-interval*)
      (pipeline-rewrite-source))))

(defun configure-optimization (level &key (specialization-threshold 5) (source-rewrite-threshold 50) (source-rewrite-interval 100))
  "Configure the unified optimization pipeline.
   LEVEL is 1 (cache only), 2 (+ specialization), or 3 (+ source rewrite).
   Keyword arguments tune the thresholds for each level."
  (setf *pipeline-level* level)
  (setf *pipeline-specialization-threshold* specialization-threshold)
  (setf *pipeline-source-rewrite-threshold* source-rewrite-threshold)
  (setf *pipeline-source-rewrite-interval* source-rewrite-interval)
  (cond
    ((>= level 3)
     (pipeline-enable-specialization)
     (setf *cache-set-hook* #'pipeline-cache-set-hook)
     (setf *auto-persist-cache* t)
     (format t "Optimization pipeline: Level 3 (cache + specialization + source rewrite + persistence).~%"))
    ((>= level 2)
     (pipeline-enable-specialization)
     (setf *cache-set-hook* #'pipeline-cache-set-hook)
     (setf *auto-persist-cache* nil)
     (format t "Optimization pipeline: Level 2 (cache + specialization).~%"))
    (t
     (setf *cache-set-hook* nil)
     (setf *auto-persist-cache* nil)
     (format t "Optimization pipeline: Level 1 (cache only).~%")))
  level)
