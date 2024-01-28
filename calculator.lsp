;; Function to split input
(defun split-sequence (str)
  ;; Splits the string str up into three parts
  ;; 
  (let ((op-pos (position-if (lambda (c) (member c '(#\+ #\- #\* #\/))) str)))
    ;; checks if the char c is one of the operators
    (if op-pos
      ;; if true, extract the operands and operator
      (list
        (subseq str 0 op-pos)
        (string (char str op-pos))
        (subseq str (1+ op-pos))
      )
      ;; if false, signals an error
      (error "No operator found")
    )
  )
)

;; Basic arithmetic functions
(defun add (a b) (+ a b)) ;;addition
(defun subtract (a b) (- a b)) ;;subtraction
(defun multiply (a b) (* a b)) ;;multiplication
(defun divide (a b) (/ a b)) ;;division

;; Calculator function to dispatch operations
(defun calculate (operation a b)
  (cond ;; determines which arithmetic function to use
    ((equal operation "+") (add a b))
    ((equal operation "-") (subtract a b))
    ((equal operation "*") (multiply a b))
    ((equal operation "/") (divide a b))
    (t (error "Unknown operation"))
  )
)

;; Function to parse the input
(defun parse-expression (expr)
  ;; Parsing the string expression
  (let ((parts (split-sequence expr))) ; Corrected call to split-sequence
    ;; parts split as: 1st operand, operator, 2nd operand
    (list 
      (second parts) ; operator (string)
      (parse-integer (first parts)) ; 1st operand (integer)
      (parse-integer (third parts)) ; 2nd operand (integer)
    )
  )
)

;; Main function to handle command line arguments
(defun main ()
  ;; Checks if there are arguments and calculates the result
  ;; checks if there is at least one argument
  (if (> (length *args*) 0)
    ;; process the input
    (let* (
        (expr (first *args*))
        (parsed-expr (parse-expression expr)) ;; bind the result of parse-expression to parsed-expr
        (operation (first parsed-expr)) ;; extract the operation and operands
        (operand-a (second parsed-expr))
        (operand-b (third parsed-expr))
      )
      ;; calculate and print the result
      (format t "~A~%" (calculate operation operand-a operand-b))
    )
    ;; if the condition is false, print usage information
    (format t "Usage: clisp calculator.lisp '<expression>'~%")
  )
)

;; Call main if there are arguments
(main)
