;;;; matcher.lisp
;;;; Pattern matching and sub-expression replacement for cached AST nodes.
;;;;
;;;; Given an AST node and a cache, the matcher rewrites the node by replacing
;;;; any cached sub-tree with a :constant node holding the cached value. This
;;;; lets the evaluator skip already-computed sub-expressions entirely.

(in-package :self-modifying-calculator)

(defun rewrite-with-cache (node cache)
  "Rewrite NODE by replacing any cached sub-tree with a constant node.
   Returns the rewritten node (which may still contain uncached sub-trees)."
  (multiple-value-bind (value found) (cache-get cache node)
    (if found
        (constant-node value)
        (cond
          ((constant-node-p node) node)
          ((variable-node-p node) node)
          ((consp node)
           (cons (car node)
                 (mapcar (lambda (child) (rewrite-with-cache child cache)) (cdr node))))
          (t node)))))

(defun rewrite-with-cache-until-stable (node cache)
  "Rewrite NODE repeatedly until no more cached sub-tree replacements can be made.
   This is currently equivalent to a single rewrite, but the hook is here for
   future extensions (e.g., conditional rewrites)."
  (rewrite-with-cache node cache))
