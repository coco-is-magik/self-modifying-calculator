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

(defun ast-operator-p (node)
  "Return true if NODE is an operator AST node."
  (and (consp node) (symbolp (car node)) (not (ast-constant-p node))))

(defun ast-op (node)
  "Return the operator symbol of an operator AST node."
  (car node))

(defun ast-args (node)
  "Return the arguments of an operator AST node."
  (cdr node))

(defun c-operator (op)
  "Map a Lisp operator keyword to a C infix operator string."
  (case op
    (:+ "+")
    (:- "-")
    (:* "*")
    (:/ "/")
    (:^ "pow")
    (otherwise nil)))

(defun emit-c-expression (node)
  "Emit a C expression string for a ground AST NODE."
  (cond
    ((ast-constant-p node)
     (let ((v (ast-constant-value node)))
       (if (integerp v)
           (format nil "~D.0" v)
           (format nil "~F" (coerce v 'double-float)))))
    ((ast-operator-p node)
     (let ((op (ast-op node))
           (args (ast-args node)))
       (cond
         ((eq op :vec3)
          ;; Represent vec3 as a struct literal. The generated runtime does not
          ;; yet support vector return values, so this is emitted as a comment.
          (format nil "/* vec3(~{~A~^, ~}) */ 1.0" (mapcar #'emit-c-expression args)))
         ((and (eq op :^) (= (length args) 2))
          (format nil "pow(~A, ~A)"
                  (emit-c-expression (first args))
                  (emit-c-expression (second args))))
         ((member op '(:+ :*))
          (format nil "(~{~A~^ ~A ~})"
                  (loop for arg in args
                        for i from 0
                        collect (emit-c-expression arg)
                        when (< i (1- (length args)))
                        collect (c-operator op))))
         ((member op '(:- :/))
          (if (= (length args) 1)
              (format nil "-(~A)" (emit-c-expression (first args)))
              (format nil "(~{~A~^ ~A ~})"
                      (loop for arg in args
                            for i from 0
                            collect (emit-c-expression arg)
                            when (< i (1- (length args)))
                            collect (c-operator op)))))
         (t
          ;; Generic function call syntax for other operators.
          (format nil "~A(~{~A~^, ~})"
                  (string-downcase (symbol-name op))
                  (mapcar #'emit-c-expression args))))))
    (t
     (error "Cannot emit C expression for node: ~S" node))))

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
            do (let ((c-expr (emit-c-expression ast)))
                 (format stream "int smc_expr_~A(double *out) {~%"
                         (c-identifier expr))
                 (format stream "    *out = ~A;~%" c-expr)
                 (format stream "    return SMC_OK;~%")
                 (format stream "}~%~%")))

      ;; Generic dispatch by ID.
      (format stream "/* Generic dispatch by stable expression ID */~%")
      (format stream "int smc_call_double(smc_expr_id_t expr_id,~%")
      (format stream "                    const double *args, size_t argc,~%")
      (format stream "                    double *out) {~%")
      (format stream "    (void)args;~%")
      (format stream "    (void)argc;~%")
      (format stream "    switch (expr_id) {~%")
      (loop for expr in exprs
            for id from 1
            do (format stream "        case SMC_EXPR_~A: return smc_expr_~A(out);~%"
                       (string-upcase (c-identifier expr))
                       (c-identifier expr)))
      (format stream "        default: return SMC_ERR_INVALID;~%")
      (format stream "    }~%")
      (format stream "}~%~%")

      ;; Metadata helpers.
      (format stream "/* Expression metadata */~%")
      (format stream "int smc_expr_count(void) {~%")
      (format stream "    return SMC_EXPR_COUNT;~%")
      (format stream "}~%~%")

      (format stream "size_t smc_expr_arity(smc_expr_id_t id) {~%")
      (format stream "    (void)id;~%")
      (format stream "    return 0;  /* all generated expressions are ground */~%")
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
