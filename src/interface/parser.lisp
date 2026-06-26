;;;; parser.lisp
;;;; A simple parser for mathematical expressions.
;;;;
;;;; Supports the operators +, -, *, /, ^ with conventional precedence.
;;;; Numbers become :constant nodes. Letters become :variable nodes.
;;;; Parentheses are supported.

(in-package :self-modifying-calculator)

;;; Tokenizer

(defun tokenize (string)
  "Convert STRING into a list of tokens (numbers, symbols, operators, parens)."
  (let ((tokens '())
        (i 0)
        (len (length string)))
    (labels ((peek ()
               (if (< i len) (char string i) nil))
             (advance ()
               (let ((c (char string i))) (incf i) c))
             (read-number ()
               (let ((start i))
                 (loop while (and (peek) (digit-char-p (peek))) do (advance))
                 (when (and (peek) (char= (peek) #\.))
                   (advance)
                   (loop while (and (peek) (digit-char-p (peek))) do (advance)))
                 (parse-number (subseq string start i))))
             (read-identifier ()
               (let ((start i))
                 (loop while (and (peek) (alpha-char-p (peek))) do (advance))
                 (intern (string-upcase (subseq string start i)) :keyword))))
      (loop while (< i len) do
        (let ((c (peek)))
          (cond
            ((member c '(#\space #\tab #\newline)) (advance))
            ((digit-char-p c) (push (read-number) tokens))
            ((alpha-char-p c) (push (read-identifier) tokens))
            ((member c '(#\+ #\- #\* #\/ #\^ #\=))
             (push (intern (string (advance)) :keyword) tokens))
            ((char= c #\() (push :lparen tokens) (advance))
            ((char= c #\)) (push :rparen tokens) (advance))
            (t (error "Unexpected character: ~A" c))))))
    (nreverse tokens)))

(defun parse-number (string)
  "Parse STRING as either an integer or a float."
  (if (find #\. string)
      (parse-float string)
      (parse-integer string)))

(defun parse-float (string)
  "Parse a decimal string into a float."
  (let* ((dot-pos (position #\. string))
         (whole (parse-integer (subseq string 0 dot-pos)))
         (frac-str (subseq string (1+ dot-pos)))
         (frac-len (length frac-str))
         (frac (parse-integer frac-str)))
    (+ whole (/ frac (expt 10 frac-len)))))

;;; Token stream

(defstruct token-stream tokens)

(defun peek-token (stream)
  (car (token-stream-tokens stream)))

(defun next-token (stream)
  (pop (token-stream-tokens stream)))

(defun expect-token (stream token)
  (let ((actual (next-token stream)))
    (unless (eq actual token)
      (error "Expected token ~A but found ~A" token actual))))

;;; Recursive descent parser with precedence

(defun parse-expression (stream)
  "Parse from a token stream into an AST."
  (parse-addition stream))

(defun parse-addition (stream)
  (let ((left (parse-multiplication stream)))
    (loop while (member (peek-token stream) '(:+ :-)) do
      (let ((op (next-token stream)))
        (setq left (make-ast op left (parse-multiplication stream)))))
    left))

(defun parse-multiplication (stream)
  (let ((left (parse-power stream)))
    (loop while (member (peek-token stream) '(:* :/)) do
      (let ((op (next-token stream)))
        (setq left (make-ast op left (parse-power stream)))))
    left))

(defun parse-power (stream)
  (let ((left (parse-unary stream)))
    (loop while (eq (peek-token stream) :^) do
      (next-token stream)
      (setq left (make-ast :^ left (parse-unary stream))))
    left))

(defun parse-unary (stream)
  (cond
    ((eq (peek-token stream) :-)
     (next-token stream)
     (make-ast :- (constant-node 0) (parse-unary stream)))
    ((eq (peek-token stream) :+)
     (next-token stream)
     (parse-unary stream))
    (t (parse-primary stream))))

(defun parse-primary (stream)
  (let ((token (peek-token stream)))
    (cond
      ((null token) (error "Unexpected end of input"))
      ((numberp token)
       (next-token stream)
       (constant-node token))
      ((keywordp token)
       (cond
         ((eq token :lparen)
          (next-token stream)
          (let ((expr (parse-expression stream)))
            (expect-token stream :rparen)
            expr))
         (t (next-token stream)
            (variable-node token))))
      (t (error "Unexpected token: ~A" token)))))

;;; Parser cache

(defparameter *parse-cache* (make-hash-table :test 'equal)
  "Cache mapping input strings to parsed ASTs.")

(defun parse (string)
  "Parse STRING into an AST node. Results are cached for repeated strings."
  (or (gethash string *parse-cache*)
      (let ((ast (let ((stream (make-token-stream :tokens (tokenize string))))
                   (parse-expression stream))))
        (setf (gethash string *parse-cache*) ast)
        ast)))

(defun clear-parse-cache ()
  "Clear the parser cache."
  (clrhash *parse-cache*)
  nil)
