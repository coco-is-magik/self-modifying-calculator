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
  (test-no-internal-package-access)
  (finish-output *trace-output*)
  (write-line "All generator tests passed." *error-output*)
  t)

(run-generator-tests)
