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

(defun canonical-cache-key (ast)
  "Return a stable canonical key for AST: a cons of (ast-string . arity)."
  (cons (smc::ast-to-string ast) (expression-arity ast)))

(defun collect-cache-entries (cache)
  "Return a list of (ast . value) entries from CACHE, sorted by canonical key."
  (let ((entries '()))
    (maphash (lambda (key value)
               (push (cons key value) entries))
             (slot-value cache 'self-modifying-calculator::table))
    (sort entries (lambda (a b)
                    (let ((ka (canonical-cache-key (car a)))
                          (kb (canonical-cache-key (car b))))
                      (or (string< (car ka) (car kb))
                          (and (string= (car ka) (car kb))
                               (< (cdr ka) (cdr kb)))))))))

(defun stable-cache-hash (entries)
  "Compute a reproducible hash of the sorted canonical keys of ENTRIES.
   This is a simple string hash, not cryptographic, intended only for
   detecting changes between generator runs."
  (let ((hash 5381))
    (dolist (entry entries)
      (let ((key (canonical-cache-key (car entry))))
        (loop for c across (car key)
              do (setf hash (logand (+ (* hash 33) (char-code c)) #xFFFFFFFF)))
        (loop for c across (format nil "~D" (cdr key))
              do (setf hash (logand (+ (* hash 33) (char-code c)) #xFFFFFFFF)))))
    (format nil "~8,'0X" hash)))

(defun unique-c-identifier (expr-string existing)
  "Return a C identifier for EXPR-STRING that does not collide with EXISTING."
  (let ((base (c-identifier expr-string)))
    (if (not (member base existing :test #'string=))
        base
        (loop for i from 1
              for candidate = (format nil "~A_~D" base i)
              until (not (member candidate existing :test #'string=))
              finally (return candidate)))))

(defun c-escape-comment (s)
  "Escape a string for safe inclusion in a C block comment."
  (with-output-to-string (out)
    (loop for c across s
          do (case c
               (#\* (write-string "*" out))
               (#\/ (write-string "/" out))
               (otherwise (write-char c out))))))

(defun generate-c-source (output-path &key (cache smc:*global-cache*))
  "Generate a C source file at OUTPUT-PATH from CACHE."
  (let* ((entries (collect-cache-entries cache))
         (exprs (mapcar (lambda (entry) (smc::ast-to-string (car entry))) entries))
         (id-map (make-hash-table :test 'equal))
         (identifiers (mapcar (lambda (expr) (c-identifier expr)) exprs)))

    ;; Resolve identifier collisions deterministically.
    (let ((seen '())
          (unique '()))
      (dolist (base identifiers)
        (let ((uid (unique-c-identifier base seen)))
          (push uid seen)
          (push uid unique)))
      (setf identifiers (nreverse unique)))

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
      (format stream "/* This file is machine-generated. Do not hand-edit. */~%")
      (format stream "/*~%")
      (format stream " * SMC_GENERATED_ABI_VERSION: ~D~%" 1)
      (format stream " * SMC_GENERATED_GENERATOR_VERSION: 0.2.0~%")
      (format stream " * SMC_GENERATED_CACHE_HASH: ~A~%" (stable-cache-hash entries))
      (format stream " * SMC_GENERATED_EXPR_COUNT: ~D~%" (length exprs))
      (format stream " * SMC_GENERATED_BUILD_ID: ~A~%" (or (sb-ext:posix-getenv "SMC_BUILD_ID") ""))
      (format stream " */~%~%")
      (format stream "#include \"smc.h\"~%")
      (format stream "#include <math.h>~%~%")
      (format stream "#define SMC_GENERATED_ABI_VERSION ~D~%" 1)
      (format stream "#define SMC_GENERATED_GENERATOR_VERSION \"0.2.0\"~%")
      (format stream "#define SMC_GENERATED_CACHE_HASH \"~A\"~%" (stable-cache-hash entries))
      (format stream "#define SMC_GENERATED_EXPR_COUNT ~D~%" (length exprs))
      (format stream "#define SMC_GENERATED_BUILD_ID \"~A\"~%~%"
              (c-escape-comment (or (sb-ext:posix-getenv "SMC_BUILD_ID") "")))

      ;; Runtime ABI-version check symbol.
      (format stream "/* Runtime ABI-version check */~%")
      (format stream "int smc_generated_abi_version(void) {~%")
      (format stream "    return SMC_GENERATED_ABI_VERSION;~%")
      (format stream "}~%~%")

      ;; Stable expression IDs.
      (format stream "/* Stable expression IDs */~%")
      (loop for expr in exprs
            for id in identifiers
            for n from 1
            do (format stream "#define SMC_EXPR_~A ~D~%"
                       (string-upcase id)
                       n))
      (format stream "#define SMC_EXPR_COUNT ~D~%~%" (length exprs))

      ;; Named wrappers for direct calls.
      (format stream "/* Named wrappers for direct calls */~%")
      (loop for (ast . value) in entries
            for expr in exprs
            for id in identifiers
            do (let ((c-expr (emit-c-expression ast ast)))
                 (format stream "int smc_expr_~A(const double *args, double *out) {~%"
                         id)
                 (format stream "    (void)args;~%")
                 (format stream "    *out = ~A;~%" c-expr)
                 (format stream "    return SMC_OK;~%")
                 (format stream "}~%~%")))

      ;; Generic dispatch by ID.
      (format stream "/* Generic dispatch by stable expression ID */~%")
      (format stream "int smc_call_double(smc_expr_id_t expr_id,~%")
      (format stream "                    const double *args, size_t argc,~%")
      (format stream "                    double *out) {~%")
      (format stream "    extern smc_stats_t smc_global_stats;~%")
      (format stream "    smc_global_stats.total_calls++;~%")
      (format stream "    if (out == NULL) {~%")
      (format stream "        smc_global_stats.invalid_calls++;~%")
      (format stream "        return SMC_ERR_INVALID;~%")
      (format stream "    }~%")
      (format stream "    switch (expr_id) {~%")
      (loop for (ast . value) in entries
            for expr in exprs
            for id in identifiers
            do (let ((arity (expression-arity ast)))
                 (format stream "        case SMC_EXPR_~A:~%"
                         (string-upcase id))
                 (format stream "            if (argc != ~D) {~%" arity)
                 (format stream "                smc_global_stats.arity_errors++;~%")
                 (format stream "                return SMC_ERR_ARITY;~%")
                 (format stream "            }~%")
                 (format stream "            smc_global_stats.generated_hits++;~%")
                 (format stream "            return smc_expr_~A(args, out);~%"
                         id)))
      (format stream "        default:~%")
      (format stream "            smc_global_stats.invalid_ids++;~%")
      (format stream "            return SMC_ERR_NOT_FOUND;~%")
      (format stream "    }~%")
      (format stream "}~%~%")

      ;; Metadata helpers.
      (format stream "/* Expression metadata */~%")
      (format stream "int smc_expr_count(void) {~%")
      (format stream "    return SMC_GENERATED_EXPR_COUNT;~%")
      (format stream "}~%~%")

      (format stream "size_t smc_expr_arity(smc_expr_id_t id) {~%")
      (format stream "    switch (id) {~%")
      (loop for (ast . value) in entries
            for id in identifiers
            do (format stream "        case SMC_EXPR_~A: return ~D;~%"
                       (string-upcase id)
                       (expression-arity ast)))
      (format stream "        default: return 0;~%")
      (format stream "    }~%")
      (format stream "}~%~%")

      (format stream "const char *smc_expr_source(smc_expr_id_t id) {~%")
      (format stream "    switch (id) {~%")
      (loop for expr in exprs
            for id in identifiers
            do (format stream "        case SMC_EXPR_~A: return ~A;~%"
                       (string-upcase id)
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

;; Run main only when invoked as a script, not when loaded as a library.
(when (and sb-ext:*posix-argv* (> (length sb-ext:*posix-argv*) 1))
  (main))
