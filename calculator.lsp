;; Define the basic arithmetic functions
(defun add (a b)
  (+ a b))

(defun subtract (a b)
  (- a b))

(defun multiply (a b)
  (* a b))

(defun divide (a b)
  (/ a b))

;; Calculator function to dispatch operations
(defun calculate (operation a b)
  (cond
    ((equal operation "+") (add a b))
    ((equal operation "-") (subtract a b))
    ((equal operation "*") (multiply a b))
    ((equal operation "/") (divide a b))
    (t (error "Unknown operation"))))

;; Main function to handle command line arguments
(defun main ()
  (if (> (length *args*) 2)
    (let ((operation (nth 0 *args*))
          (a (parse-integer (nth 1 *args*)))
          (b (parse-integer (nth 2 *args*))))
      (format t "~A~%" (calculate operation a b)))
    (format t "Usage: clisp calculator.lisp <operation> <num1> <num2>~%")))

;; Call main if there are arguments
(main)
