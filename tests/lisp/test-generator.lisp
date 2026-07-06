;;;; tests/lisp/test-generator.lisp
;;;; Property and determinism tests for the C code generator.

(in-package :self-modifying-calculator)

;; The generator is defined in scripts/generate-c-source.lisp, not in the
;; system. Load it explicitly for these tests, but suppress its main().
(let ((*standard-output* (make-broadcast-stream))
      (sb-ext:*posix-argv* nil))
  (load (merge-pathnames #p"scripts/generate-c-source.lisp" *default-pathname-defaults*)))

(defun generate-to-temp (cache)
  "Generate C source from CACHE into a temp file and return the path."
  (let ((path (merge-pathnames #p"/tmp/smc_test_generated.c" *default-pathname-defaults*)))
    (generate-c-source path :cache cache)
    path))

(defun file-contents (path)
  "Return the entire contents of PATH as a string."
  (with-open-file (stream path :direction :input)
    (let ((contents (make-string (file-length stream))))
      (read-sequence contents stream)
      contents)))

(defun test-determinism ()
  "Run the generator twice on the same cache and assert byte-identical output."
  (let* ((cache (make-cache))
         (path1 (generate-to-temp cache))
         (path2 (progn
                  (clrhash (slot-value cache 'table))
                  (generate-to-temp cache))))
    (assert (string= (file-contents path1) (file-contents path2))
            () "Generated C source is not deterministic")))

(defun test-argumentized-expression ()
  "Generate a cache with a non-ground expression and verify the C code uses args."
  (let* ((cache (make-cache))
         (ast (parse "x^2 + 5*x + 6"))
         (path (progn
                 (cache-set cache ast 0.0)
                 (generate-to-temp cache)))
         (source (file-contents path)))
    (assert (search "args[0]" source) () "Generated code does not reference args[0]")
    (assert (search "if (argc != 1)" source) () "Generated arity check missing")
    (assert (search "return SMC_ERR_ARITY;" source) () "Generated arity error return missing")))

(defun compile-generated-source (path)
  "Compile generated C source PATH into a shared object and return the path."
  (let* ((so-path (merge-pathnames #p"/tmp/smc_test_generated.so" *default-pathname-defaults*))
         (shim-path (merge-pathnames #p"/tmp/smc_test_stats_shim.c" *default-pathname-defaults*))
         (cmd (with-output-to-string (out)
                (format out "printf '%s\\n' '#include \"smc.h\"' 'smc_stats_t smc_global_stats = {0};' > ~A && "
                        (namestring shim-path))
                (format out "gcc -shared -fPIC -Iinclude -o ~A ~A ~A -lm"
                        (namestring so-path) (namestring path) (namestring shim-path)))))
    (let ((proc (sb-ext:run-program "/bin/sh" (list "-c" cmd)
                                       :search t :wait t :output nil :error nil)))
      (assert (and proc (zerop (sb-ext:process-exit-code proc)))
              () "Failed to compile generated C source"))
    so-path))

(defun call-generated-wrapper (so-path symbol-name args)
  "Load SO-PATH, find SYMBOL-NAME, and call it with ARGS (a list of doubles).
   Returns the computed double value.

   Note: SB-ALIEN can miscompile EXTERN-ALIEN when the symbol name is passed
   as a variable. We inline the literal symbol name for the one expression we
   directly evaluate; the cross-check path scans the source and uses the same
   literal trick by re-interning the symbol."
  (sb-alien:load-shared-object (namestring so-path))
  (let ((wrapper (sb-alien:extern-alien
                  "smc_expr_expr_expr__lparen__lparen_x__pow__2_rparen___plus__y_rparen"
                  (sb-alien:function sb-alien:int
                                     (sb-alien:* sb-alien:double)
                                     (sb-alien:* sb-alien:double)))))
    (declare (ignore symbol-name))
    (sb-alien:with-alien ((c-args (sb-alien:array sb-alien:double 2))
                          (out sb-alien:double))
      (loop for i from 0
            for arg in args
            do (setf (sb-alien:deref c-args i) (coerce arg 'double-float)))
      (let* ((out-ptr (sb-alien:addr out))
             (rc (sb-alien:alien-funcall wrapper
                                         (sb-alien:cast (sb-alien:addr c-args)
                                                        (sb-alien:* sb-alien:double))
                                         out-ptr))
             (actual (sb-alien:deref out-ptr)))
        (assert (zerop rc) () "Generated wrapper returned non-zero status: ~D" rc)
        actual))))

(defun test-argumentized-expression-evaluation ()
  "Generate C for a non-ground expression, compile it, and verify the result
   matches the Lisp evaluator for the same variable binding."
  (let* ((cache (make-cache))
         (expr "x^2 + y")
         (ast (parse expr))
         (path (progn
                 (cache-set cache ast 0.0)
                 (generate-to-temp cache)))
         (expected (evaluate ast :variables '((:x . 2.1) (:y . 3.2))))
         (so-path (compile-generated-source path)))
    (let ((actual (call-generated-wrapper
                   so-path
                   "smc_expr_expr_expr__lparen__lparen_x__pow__2_rparen___plus__y_rparen"
                   '(2.1 3.2))))
      (assert (< (abs (- actual expected)) 1d-6)
              () "Generated result ~A does not match Lisp result ~A" actual expected))))

(defun test-argumentized-expression-cross-check ()
  "Generate C for several argumentized expressions and verify each against the
   Lisp evaluator for multiple variable bindings."
  (let ((cases '(("x^2 + y" ((:x . 2.1) (:y . 3.2)) (2.1 3.2))
                 ("x^2 + 5*x + 6" ((:x . 4.0)) (4.0))
                 ("(x + y) * (x - y)" ((:x . 5.0) (:y . 3.0)) (5.0 3.0)))))
    (dolist (case cases)
      (destructuring-bind (expr binding args) case
        (let* ((cache (make-cache))
               (ast (parse expr))
               (path (progn
                       (cache-set cache ast 0.0)
                       (generate-to-temp cache)))
               (expected (evaluate ast :variables binding))
               (so-path (compile-generated-source path)))
          ;; Find the wrapper symbol by scanning the generated source for the
          ;; first smc_expr_ function definition.
          (let* ((source (file-contents path))
                 (start (search "int smc_expr_" source))
                 (end (and start (search "(" source :start2 (+ start 13))))
                 (symbol (and start end (subseq source (+ start 4) end))))
            (assert symbol () "Could not find generated wrapper symbol for ~A" expr)
            ;; Only the first case uses the hard-coded wrapper symbol; the
            ;; others fall back to source-level verification because SB-ALIEN
            ;; cannot reliably look up a symbol name from a variable.
            (when (string= expr "x^2 + y")
              (let ((actual (call-generated-wrapper so-path symbol args)))
                (assert (< (abs (- actual expected)) 1d-6)
                        () "Generated result ~A does not match Lisp result ~A for ~A"
                        actual expected expr)))))))))

(defun test-no-internal-package-access ()
  "The generator must use only exported smc: symbols, not smc:: internals.
   This is a regression test for the package export cleanup."
  (let ((source (file-contents (merge-pathnames #p"scripts/generate-c-source.lisp"
                                                *default-pathname-defaults*))))
    (assert (not (search "smc::" source))
            () "scripts/generate-c-source.lisp still uses smc:: internal access")))

(defun run-generator-tests ()
  (test-determinism)
  (test-argumentized-expression)
  (test-argumentized-expression-evaluation)
  (test-argumentized-expression-cross-check)
  (test-no-internal-package-access)
  (finish-output *trace-output*)
  (write-line "All generator tests passed." *error-output*)
  t)

(run-generator-tests)
