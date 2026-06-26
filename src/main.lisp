;;;; main.lisp
;;;; Entry point for the self-modifying calculator.

(in-package :self-modifying-calculator)

(defun run-calculator (expression-string)
  "Parse and evaluate an expression string, returning the result."
  (let ((ast (parse expression-string)))
    (evaluate ast)))

(defun main (&optional args)
  "CLI entry point. ARGS is a list of command-line argument strings."
  (let ((args (or args (cdr sb-ext:*posix-argv*))))
    (if args
        (let ((expression (join-strings args " ")))
          (handler-case
              (format t "~A~%" (run-calculator expression))
            (error (e)
              (format *error-output* "Error: ~A~%" e)
              (sb-ext:exit :code 1))))
        (progn
          (format t "Usage: sbcl --load main.lisp --eval '(smc:main \"<expression>\")'~%")
          (format t "   or: sbcl --script main.lisp \"<expression>\"~%")))))

(defun join-strings (strings separator)
  "Join STRINGS with SEPARATOR."
  (if (null strings)
      ""
      (reduce (lambda (a b) (concatenate 'string a separator b)) strings)))

;; Load the arithmetic module and any other modules by default.
(register-arithmetic-operators)

;; Note: this file is intended to be loaded as a library. The CLI entry point
;; is invoked explicitly by run.sh or by the user with (smc:main args).
