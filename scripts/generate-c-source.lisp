;;;; scripts/generate-c-source.lisp — build-time C code generator for SMC
;;;;
;;;; Usage:
;;;;   sbcl --script scripts/generate-c-source.lisp <output-file>
;;;;
;;;; This script loads the SMC system, warms up the cache on a representative
;;;; set of expressions, and emits a C source file containing:
;;;;   - Stable integer IDs for each cached expression
;;;;   - Named wrapper functions for direct calls
;;;;   - A generic smc_call_double dispatch by ID
;;;;   - Metadata helpers: smc_expr_count, smc_expr_arity, smc_expr_source
;;;;
;;;; The generated file is intended to be compiled into a host program for
;;;; the production hot path. It has no runtime SBCL dependency.

(require :asdf)

(pushnew *default-pathname-defaults* asdf:*central-registry*)

(asdf:load-system :self-modifying-calculator)

(defun c-identifier (expr-string)
  "Create a C-friendly identifier from an expression string.
   The result is a valid C identifier that is unlikely to collide."
  (let* ((s (string-downcase expr-string))
         (raw (with-output-to-string (out)
                (write-string "expr_" out)
                (loop for c across s
                      do (cond
                           ((or (char<= #\a c #\z)
                                (char<= #\0 c #\9))
                            (write-char c out))
                           ((char= c #\+)
                            (write-string "_plus_" out))
                           ((char= c #\-)
                            (write-string "_minus_" out))
                           ((char= c #\*)
                            (write-string "_mul_" out))
                           ((char= c #\/)
                            (write-string "_div_" out))
                           ((char= c #\^)
                            (write-string "_pow_" out))
                           ((char= c #\()
                            (write-string "_lparen_" out))
                           ((char= c #\))
                            (write-string "_rparen_" out))
                           ((char= c #\.)
                            (write-string "_dot_" out))
                            ((char= c #\space)
                             (write-string "_" out))
                            (t
                             (write-string "_" out))))))
          (result (string-trim "_" raw)))
    (if (zerop (length result))
        "expr"
        result)))

(defun c-escape-string (s)
  "Escape a Lisp string for use as a C string literal."
  (with-output-to-string (out)
    (write-char #\" out)
    (loop for c across s
          do (case c
               (#\\ (write-string "\\\\" out))
               (#\" (write-string "\\\"" out))
               (#\newline (write-string "\\n" out))
               (#\tab (write-string "\\t" out))
               (otherwise (write-char c out))))
    (write-char #\" out)))

(defun ast-constant-p (node)
  "Return true if NODE is a constant AST node."
  (and (consp node) (eq (car node) :constant)))

(defun ast-constant-value (node)
  "Return the value of a constant AST node."
  (second node))

(defun ast-variable-p (node)
  "Return true if NODE is a variable AST node."
  (and (consp node) (eq (car node) :variable)))

(defun ast-variable-name (node)
  "Return the name of a variable AST node."
  (second node))

(defun ast-operator-p (node)
  "Return true if NODE is an operator AST node."
  (and (consp node) (symbolp (car node)) (not (ast-constant-p node)) (not (ast-variable-p node))))

(defun ast-op (node)
  "Return the operator symbol of an operator AST node."
  (car node))

(defun ast-args (node)
  "Return the arguments of an operator AST node."
  (cdr node))

(defun expression-variables (node)
  "Return the ordered, deduplicated list of variable names appearing in NODE."
  (let ((seen '()))
    (labels ((walk (n)
               (cond
                 ((ast-variable-p n)
                  (let ((name (ast-variable-name n)))
                    (unless (member name seen)
                      (setf seen (append seen (list name))))))
                 ((consp n)
                  (dolist (child (cdr n))
                    (walk child))))))
      (walk node)
      seen)))

(defun expression-arity (node)
  "Return the number of free variables in NODE."
  (length (expression-variables node)))

(defun variable-index (node var)
  "Return the 0-based argument index of VAR in NODE's variable ordering."
  (position var (expression-variables node)))

(defun c-operator (op)
  "Map a Lisp operator keyword to a C infix operator string."
  (case op
    (:+ "+")
    (:- "-")
    (:* "*")
    (:/ "/")
    (:^ "pow")
    (otherwise nil)))

(defun emit-c-expression (node root)
  "Emit a C expression string for AST NODE. ROOT is the top-level expression
   used to resolve variable argument indices."
  (cond
    ((ast-constant-p node)
     (let ((v (ast-constant-value node)))
       (if (integerp v)
           (format nil "~D.0" v)
           (format nil "~F" (coerce v 'double-float)))))
    ((ast-variable-p node)
     (let ((idx (variable-index root (ast-variable-name node))))
       (format nil "args[~D]" idx)))
    ((ast-operator-p node)
     (let ((op (ast-op node))
           (args (ast-args node)))
       (cond
         ((eq op :vec3)
          ;; Represent vec3 as a struct literal. The generated runtime does not
          ;; yet support vector return values, so this is emitted as a comment.
          (format nil "/* vec3(~{~A~^, ~}) */ 1.0" (mapcar (lambda (a) (emit-c-expression a root)) args)))
         ((and (eq op :^) (= (length args) 2))
          (format nil "pow(~A, ~A)"
                  (emit-c-expression (first args) root)
                  (emit-c-expression (second args) root)))
         ((member op '(:+ :*))
          (format nil "(~{~A~^ ~A ~})"
                  (loop for arg in args
                        for i from 0
                        collect (emit-c-expression arg root)
                        when (< i (1- (length args)))
                        collect (c-operator op))))
         ((member op '(:- :/))
          (if (= (length args) 1)
              (format nil "-(~A)" (emit-c-expression (first args) root))
              (format nil "(~{~A~^ ~A ~})"
                      (loop for arg in args
                            for i from 0
                            collect (emit-c-expression arg root)
                            when (< i (1- (length args)))
                            collect (c-operator op)))))
         (t
          ;; Generic function call syntax for other operators.
          (format nil "~A(~{~A~^, ~})"
                  (string-downcase (symbol-name op))
                  (mapcar (lambda (a) (emit-c-expression a root)) args))))))
    (t
     (error "Cannot emit C expression for node: ~S" node))))

(defun emit-c-expression-ground (node)
  "Emit a C expression string for a ground AST NODE (no variables)."
  (emit-c-expression node node))

(defun collect-cache-entries (cache)
  "Return a list of (ast . value) entries from CACHE, sorted by AST string."
  (let ((entries '()))
    (maphash (lambda (key value)
               (push (cons key value) entries))
             (slot-value cache 'self-modifying-calculator::table))
    (sort entries #'string< :key (lambda (entry) (smc::ast-to-string (car entry))))))

(defun generate-c-source (output-path &key (cache smc:*global-cache*))
  "Generate a C source file at OUTPUT-PATH from CACHE."
  (let* ((entries (collect-cache-entries cache))
         (exprs (mapcar (lambda (entry) (smc::ast-to-string (car entry))) entries))
         (id-map (make-hash-table :test 'equal)))

    ;; Assign stable IDs starting from 1.
    (loop for expr in exprs
          for id from 1
          do (setf (gethash expr id-map) id))

    (ensure-directories-exist output-path)
    (with-open-file (stream output-path
                            :direction :output
                            :if-exists :supersede
                            :if-does-not-exist :create)
      (format stream "/* smc_generated.c — generated by SMC build-time optimizer */~%")
      (format stream "/* This file is machine-generated. Do not hand-edit. */~%~%")
      (format stream "#include \"smc.h\"~%")
      (format stream "#include <math.h>~%~%")

      ;; Stable expression IDs.
      (format stream "/* Stable expression IDs */~%")
      (loop for expr in exprs
            for id from 1
            do (format stream "#define SMC_EXPR_~A ~D~%"
                       (string-upcase (c-identifier expr))
                       id))
      (format stream "#define SMC_EXPR_COUNT ~D~%~%" (length exprs))

      ;; Named wrappers for direct calls.
      (format stream "/* Named wrappers for direct calls */~%")
      (loop for (ast . value) in entries
            for expr in exprs
            for id from 1
            do (let* ((arity (expression-arity ast))
                      (c-expr (emit-c-expression ast ast)))
                 (format stream "int smc_expr_~A(const double *args, double *out) {~%"
                         (c-identifier expr))
                 (format stream "    (void)args;~%")
                 (format stream "    *out = ~A;~%" c-expr)
                 (format stream "    return SMC_OK;~%")
                 (format stream "}~%~%")))

      ;; Generic dispatch by ID.
      (format stream "/* Generic dispatch by stable expression ID */~%")
      (format stream "int smc_call_double(smc_expr_id_t expr_id,~%")
      (format stream "                    const double *args, size_t argc,~%")
      (format stream "                    double *out) {~%")
      (format stream "    switch (expr_id) {~%")
      (loop for (ast . value) in entries
            for expr in exprs
            for id from 1
            do (let ((arity (expression-arity ast)))
                 (format stream "        case SMC_EXPR_~A:~%"
                         (string-upcase (c-identifier expr)))
                 (format stream "            if (argc != ~D) return SMC_ERR_ARITY;~%" arity)
                 (format stream "            return smc_expr_~A(args, out);~%"
                         (c-identifier expr))))
      (format stream "        default: return SMC_ERR_NOT_FOUND;~%")
      (format stream "    }~%")
      (format stream "}~%~%")

      ;; Metadata helpers.
      (format stream "/* Expression metadata */~%")
      (format stream "int smc_expr_count(void) {~%")
      (format stream "    return SMC_EXPR_COUNT;~%")
      (format stream "}~%~%")

      (format stream "size_t smc_expr_arity(smc_expr_id_t id) {~%")
      (format stream "    switch (id) {~%")
      (loop for (ast . value) in entries
            for expr in exprs
            for id from 1
            do (format stream "        case SMC_EXPR_~A: return ~D;~%"
                       (string-upcase (c-identifier expr))
                       (expression-arity ast)))
      (format stream "        default: return 0;~%")
      (format stream "    }~%")
      (format stream "}~%~%")

      (format stream "const char *smc_expr_source(smc_expr_id_t id) {~%")
      (format stream "    switch (id) {~%")
      (loop for expr in exprs
            for id from 1
            do (format stream "        case SMC_EXPR_~A: return ~A;~%"
                       (string-upcase (c-identifier expr))
                       (c-escape-string expr)))
      (format stream "        default: return NULL;~%")
      (format stream "    }~%")
      (format stream "}~%"))
    output-path))

(defun warm-cache (exprs)
  "Evaluate each expression in EXPRS to populate the global cache."
  (dolist (expr exprs)
    (handler-case
        (smc:run-calculator expr)
      (error (e)
        (format *error-output* "Warning: failed to warm cache for ~S: ~A~%" expr e)))))

(defun main ()
  (let ((args (cdr sb-ext:*posix-argv*)))
    (unless args
      (format *error-output* "Usage: sbcl --script scripts/generate-c-source.lisp <output-file>~%")
      (sb-ext:exit :code 1))
    (let ((path (pathname (first args))))
      ;; Warm the cache with a representative set of scalar expressions.
      (warm-cache '("2+3*4"
                    "(2+3)*4"
                    "2^3^2"
                    "10-4/2"
                    "1.5*2"
                    "7/2"))
      (generate-c-source path)
      (format t "Generated C source: ~A (~D expressions)~%" path (smc::cache-size smc:*global-cache*))
      (sb-ext:exit :code 0))))

(main)
