;;;; Generated cache literals for Self-Modifying Calculator
;;;; This file is generated automatically. Do not hand-edit.
(in-package :self-modifying-calculator)

(let ((cache *global-cache*))
  (cache-set cache '(:+ (:CONSTANT 2) (:CONSTANT 3)) 5)
  (cache-set cache '(:* (:CONSTANT 4) (:CONSTANT 5)) 20)
  (cache-size cache))
