(defn .run-procedural.gen-bindings (params args)
  (tmpfn (bindings params args)
    (if params
      (if (= (head params) nil)
        (append (append bindings (scd params)) (list ' quote args))
        (rec
          (append (append bindings (head params)) (list ' quote (head args)))
          (tail params)
          (tail args)
        )
      )
      bindings
    )
    (() params args)
  )
)

(defn .run-procedural.last (list)
  (if list
    (if (tail list)
      (.run-procedural.last (tail list))
      (head list)
    )
  )
)

(defn .run-procedural.expandm (macro args) (do
  (assert (:callable? macro))
  (assert (:macro? macro))
  (call subs-with (list
    (.run-procedural.gen-bindings (head macro) args)
    (list ' quote (.run-procedural.last macro))
  ))
))

(defn .run-procedural.getfn (call) (do
  (assert (= (type call) ' list))
  (assert (>= (len call) 1))
  (cons (eval (head call)) (tail call))
))

(defn .run-procedural.allowed? (sym)
  (switch sym
    (case ' include T)
    (case ' let T)
    (case ' fun T)
    (default F)
  )
)

(defn .run-procedural.check-allowed (depth expr)
  (if (and depth (= (type expr) ' list) expr (= (type (head expr)) ' symbol))
    (if (.run-procedural.allowed? (head expr))
      T
      (assoc (macro (eval (head expr)))
        (if (and (:callable? macro) (:macro? macro))
          (.run-procedural.check-allowed
            (- depth 1)
            (.run-procedural.expandm macro (tail expr))
          )
          F
        )
      )
    )
    F
  )
)

; (println (.run-procedural.check-allowed 16 '(like-fun main args)))

(defn .run-procedural.display-help (prog file)
  ""
  (do
    (write file "run a procedural rholisp file.\n")
    (write file # \n)
    (write file "Usage:\n")
    (write file (&$ prog " <file> [args]\n"))
    (write file "  run procedural file\n")
  )
)

(if (< (len args) 2) (do
  (.run-procedural.display-help (head args) stderr)
  (exit 1)
))

(if (or (= (scd args) "-h") (= (scd args) "--help")) (do
  (.run-procedural.display-help (head args) stdout)
  (exit)
))

(:= args (tail args))

(def .run-procedural.file (slurp (head args)))

(def .run-procedural.prog (tmpfn (acc prog)
  (assoc (parsed (parse prog))
    (if (tail parsed)
      (rec (append acc (scd parsed)) (head parsed))
      acc
    )
  )
  (nil .run-procedural.file)
))

; (println .run-procedural.prog)

(include "procedural.lisp")

(tmpfn ()
  (if .run-procedural.prog
    (assoc (expr (head .run-procedural.prog) rest (tail .run-procedural.prog))
      (if (.run-procedural.check-allowed 4 expr)
        (do
          (eval expr)
          (:= .run-procedural.prog rest)
          (rec)
        )
        (if (and (= (type expr) ' list) expr)
          (do
            (write stderr (&$
              "Error: Construct `"
              (repr (head expr))
              "` is not allowed at the top level of a procedural file!"
              # \n
            ))
            (exit 1)
          )
          (do
            (write stderr (&$
              "Error: Bare value `"
              (repr (head expr))
              "` is not allowed at the top level of a procedural file!"
              # \n
            ))
            (exit 1)
          )
        )
      )
    )
  )
  ()
)

(exit (main args))
