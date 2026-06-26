;;;; test-runner.lisp
;;;; Lightweight dependency-free test runner.
;;;; Loads all test files and runs the registered test suites.

(in-package :self-modifying-calculator)

(defparameter *registered-test-suites* '()
  "List of function symbols that run test suites.")

(defun register-test-suite (fn-symbol)
  (pushnew fn-symbol *registered-test-suites*))

(defun run-all-tests ()
  "Run all registered test suites and report summary."
  (let ((total-pass 0)
        (total-fail 0))
    (dolist (suite-sym *registered-test-suites*)
      (multiple-value-bind (pass fail) (funcall suite-sym)
        (incf total-pass pass)
        (incf total-fail fail)))
    (format t "~%=== Test Summary ===~%")
    (format t "Passed: ~D~%" total-pass)
    (format t "Failed: ~D~%" total-fail)
    (when (> total-fail 0)
      (sb-ext:exit :code 1))
    total-pass))

(defmacro define-test-suite (name &body tests)
  `(progn
     (defun ,name ()
       (let ((pass 0) (fail 0))
         (format t "=== ~A ===~%" ',name)
         ,@(mapcar
            (lambda (test)
              `(handler-case
                   (progn
                     ,test
                     (incf pass)
                     (format t "[PASS] ~A~%" ',test))
                 (error (e)
                   (incf fail)
                   (format t "[FAIL] ~A: ~A~%" ',test e))))
            tests)
         (values pass fail)))
     (register-test-suite ',name)))
