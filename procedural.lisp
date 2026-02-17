(defm let (name value)
  "define a variable"
  (def name value)
)

(defn .fun.for.resolve-expr (expr)
  (if (and (= (type expr) ' list) expr (= (head expr) ' break))
    (do
      (assert (not (tail expr)))
      T
    )
    (.fun.resolve-expr expr)
  )
)

(defn .fun.for.transform (val)
  (if (!= val T) val)
)

(defn .fun.resolve-expr (expr)
  ;(do (println "resolving expr:" expr)
  ;(println ".fun.resolve-expr:"
  (if (and (= (type expr) ' list) expr)
    (switch (head expr)
      (case ' return (list ' list (scd expr)))
      (case ' if (do
        (assert (or (= (len expr) 3) (= (len expr) 4)))
        (if (= (len expr) 3)
          (list ' if (scd expr) (.fun.resolve-expr (nth expr 2)))
          (list ' if (scd expr) (.fun.resolve-expr (nth expr 2)) (.fun.resolve-expr (nth expr 3)))
        )
      ))
      (case ' for (do
        ; (for init cond next body...)
        (if (< (len expr) 5) (do
          (writle stderr "Expected at least four arguments supplied to for loop")
          (exit 1)
        ))
        (if (= (len expr) 5)
          (list ' env-new (list ' do
            (list ' def ' .fun.for.rec (list ' \ () (
              list ' if (nth expr 2)
              (list ' or (.fun.for.resolve-expr (nth expr 4)) (list ' do (nth expr 3) ()) '(.fun.for.rec))
            )))
            (scd expr)
            '(.fun.for.rec)
          ))
          (list ' .fun.for.transform
            (list ' env-new (list ' do
              (list ' def ' .fun.for.rec (list ' \ () (
                list ' if (nth expr 2)
                (append
                  (append
                    (cons ' or (map .fun.for.resolve-expr (tail (tail (tail (tail expr))))))
                    (list ' do (nth expr 3) ())
                  )
                  '(.fun.for.rec)
                )
              )))
              (scd expr)
              '(.fun.for.rec)
            ))
          )
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

(defn .fun.extract-val (retval)
  "extract the function's return value from the list it's wrapped in if it isn't nil"
  (if retval (head retval) retval)
)

(defm fun (name params () body)
  "define a function"
  (call defn (list
    ' name
    ' params
    "a procedural function"
    (list ' .fun.extract-val
      (list ' env-new (cons ' or (cons () (map
        .fun.resolve-expr
        ' body
      ))))
    )
  ))
)
