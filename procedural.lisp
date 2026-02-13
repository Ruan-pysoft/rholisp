(defm let (name value)
  "define a variable"
  (def name value)
)

(defn .fun.resolve-expr (expr)
  ;(do (println "resolving expr:" expr)
  ;(println ".fun.resolve-expr:"
  (if (and (= (type expr) ' list) expr)
    (switch (head expr)
      (case ' return (scd expr))
      (case ' if (do
        (assert (or (= (len expr) 3) (= (len expr) 4)))
        (if (= (len expr) 3)
          (list ' if (scd expr) (.fun.resolve-expr (nth expr 2)))
          (list ' if (scd expr) (.fun.resolve-expr (nth expr 2)) (.fun.resolve-expr (nth expr 3)))
        )
      ))
      ; TODO: (do ...)
      ; TODO: while & for loops?
      (default (list ' do expr ()))
    )
    (list ' do expr ())
  )
  ;)
  ;)
  ; (if (and (= (type expr) ' list) expr (= (head expr) ' return))
    ; (scd expr)
    ; (list ' do expr ())
  ; )
)

(defm fun (name params () body)
  "define a function"
  (call defn (list
    ' name
    ' params
    "a procedural function"
    (list ' env-new (cons ' or (cons () (map
      .fun.resolve-expr
      ' body
    ))))
  ))
)
