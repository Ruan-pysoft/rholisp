;(def .line nil)
;(:= .line (readline stdin))
;(println (type .line))
;(println ([]$ .line 0))
;(println (:= .line ([]$ .line 1 (len$ .line))))
(def a "123")
(:= a ([]$ a 1 2))
(println (repr a))

(exit)
(defn getc ()
  "  -> gets a character from stdin"
  (if (= (type .line) ' string)
    (if (= .line "")
      (do (println (repr .line) ' = (repr "")) (:= .line nil) # \n)
      (assoc (ch ([]$ .line 0)) (do
        (:= .line ([]$ .line 1 (len$ .line)))
        ch
      ))
    )
    (if (= (:= .line (readline stdin)) nil)
      (- 1)
      (getc)
    )
  )
)

(println (getc))
