;;;; main.lisp
;;;; Entry point for the self-modifying calculator.

(in-package :self-modifying-calculator)

(defun run-calculator (expression-string)
  "Parse and evaluate an expression string, returning the result."
  (let ((ast (parse expression-string)))
    (evaluate ast)))

(defun main (&optional args)
  "CLI entry point. ARGS is a list of command-line argument strings.
   If `*auto-persist-cache*' is true, the global cache is loaded on startup and saved on exit."
  (let ((args (or args (cdr sb-ext:*posix-argv*))))
    (maybe-load-global-cache)
    (if args
        (let ((expression (join-strings args " ")))
          (handler-case
              (progn
                (format t "~A~%" (run-calculator expression))
                (maybe-save-global-cache))
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
(register-algebra-operators)
(register-calculus-operators)
(register-linear-algebra-operators)
(register-trigonometry-operators)
(register-statistics-operators)

;; Note: this file is intended to be loaded as a library. The CLI entry point
;; is invoked explicitly by run.sh or by the user with (smc:main args).
