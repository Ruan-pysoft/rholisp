(include "util.lisp")

; environment: ((name value) (name value) ...)
(def root-env
  (list
    (list ' nil ())
    (list ' stdin stdin)
    (list ' stdout stdout)
    (list ' stderr stderr)
  )
)

(def envs
  (list root-env)
)

(def builtins
  (list
    (list ' + +)
    (list ' - -)
    (list ' list list)
    (list ' cons cons)
    (list ' append append)
    (list ' quote quote)
  )
)

(def prev ())

(defn find-in (name env)
  (if env
    (if (= name (head (head env)))
      (head env)
      (find-in name (tail env))
    )
    nil
  )
)

(defn find-var (name envs)
  (if envs
    (assoc (found (find-in name (head envs))) (if found
      found
      (find-var name (tail envs))
    ))
    nil
  )
)

(defn gen-bindings (params args)
  (tmpfn (bindings params args)
    (if params
      (if (= (head params) nil)
        (do
          (assert (tail params))
          (assert (= (type (scd params)) ' symbol))
          (assert (not (tail (tail params))))
          (cons (list (scd params) args) bindings)
        )
        (do
          (assert (= (type (head params)) ' symbol))
          (assert args)
          (rec
            (cons (list (head params) (head args)) bindings)
            (tail params)
            (tail args)
          )
        )
      )
      (do
        (assert (not args))
        bindings
      )
    )
    (() params args)
  )
)

(defn fetch-maybe (sym bindings)
  (if bindings
    (if (= sym (head (head bindings)))
      (scd (head bindings))
      (fetch-maybe sym (tail bindings))
    )
    sym
  )
)

(defn my-subs (bindings body)
  ; bindings: ((name val) (name val) ...)
  (switch (type body)
    (case ' symbol (fetch-maybe body bindings))
    (case ' list (map (\ (val) (my-subs bindings val)) body))
    (default body)
  )
)

(defn fn-call (fn args)
  (assoc (
      params (head fn)
      meta   (if (= (len fn) 4) (scd fn))
      macro? (if (= (len fn) 3) (scd fn) (nth fn 2))
      body   (if (= (len fn) 3) (nth fn 2) (nth fn 3))
    )
    (if macro?
      (my-eval (my-subs (gen-bindings params args) body))
      (do
        (write stderr "TODO: non-macros\n")
        (exit 1)
      )
    )
  )
)

(defn my-eval (value)
  (switch (type value)
    (case ' symbol (if (= value ' _)
      prev
      (assoc (found (find-var value envs)) (if found
        (scd found)
        (if (:= found (find-var value (list builtins)))
          (scd found)
          (do
            (write stderr (&$ "undefined symbol `" (repr value) "`\n"))
            nil
          )
        )
      ))
    ))
    (case ' list (if value (assoc (fn (my-eval (head value)) args (tail value))
      (if (and (!= (type fn) ' builtin) (!= (type fn) ' list))
        (do
          (write stderr (&$ "error: tried calling value " (repr fn) " as a function\n"))
          nil
        )
        (if (= (type fn) ' builtin)
          (call fn (if (:macro? fn) args (map my-eval args)))
          (fn-call fn (if (:macro? fn) args (map my-eval args)))
        )
      )
    )))
    (default value)
  )
)

(:= builtins (append builtins (list ' eval my-eval)))

(defn run-repl () (env-new (do
  (pstr "> ")
  (def line (readline stdin))
  (if (!= line nil) (do
    (def parsed (parse line))
    (if (tail parsed)
      (:= parsed (scd parsed))
      (do (println "invalid input!") (exit 1))
    )
    (:= prev (my-eval parsed))
    (pstr (repr prev))
    (pstr # \n)
    (run-repl)
  ))
)))

(run-repl)
