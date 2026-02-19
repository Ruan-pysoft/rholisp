(defn ask (what) (do
  (pstr what) (readline stdin))
)

(pstr (&$
  "Simple calculator program" # \n
  "Type a number at the prompt" # \n
  "Press ctrl+D to exit" # \n
))

(def inp ())
(def answer 0)

(defn first$ (string) ([]$ string 0))
(defn after$ (string n) (if (> (len$ string) n)
  ([]$ string n (len$ string))
  ""
))
(defn tail$ (string) (after$ 1))

(defn digit? (ch) (and (>= ch # 0) (<= ch # 9)))
(defn space? (ch) (and (!= ch (- 1)) (<= ch #sp)))

; program: (string position)

(defn program.advance (program () n) (do
  (assert (and (or (not n) (not (tail n))) "should only take one additional argument!"))
  (if (not n)
    (program.advance program 1)
    (list (after$ (head program) (head n)) (+ (scd program) (head n)))
  )
))

(defn program.end? (program) (not (truthy? (head program))))

(defn program.first$ (program)
  (if (program.end? program)
    (- 1)
    (first$ (head program))
  )
)

(defn trim-ws (program)
  (if (space? (program.first$ program))
    (trim-ws (program.advance program))
    program
  )
)

(defn program.err (program message) (do
  (write stderr (&$
    "Error at position " (repr (scd program)) ", here: " (head program) ".\n"
    "Error: " message
  ))
  (exit 1)
))

(defn parse-num (program)
  " -> (number program)"
  (tmpfn (res program)
    (if (digit? (program.first$ program))
      (rec
        (+ (* res 10) (- (program.first$ program) # 0))
        (program.advance program)
      )
      (list res program)
    )
    (0 program)
  )
)

(defn parse-term (program) (assoc
  (rec (\ (lhs program)
    (do
      (:= program (trim-ws program))
      (switch (program.first$ program)
        (case # + (assoc
          (rhs (parse-num (trim-ws (program.advance program))))
          (rec (+ lhs (head rhs)) (scd rhs))
        ))
        (case # - (assoc
          (rhs (parse-num (trim-ws (program.advance program))))
          (rec (- lhs (head rhs)) (scd rhs))
        ))
        (default (list lhs program))
      )
    )
  ))
  (call rec (parse-num program))
))

(defn calc-parse (string)
  (parse-term (trim-ws (list string 0)))
)
; (defn calc-parse (string) (tmpfn (program)
  ; (if (digit? (program.first$ program))
    ; (parse-num program)
    ; (program.err program "expected a numeric character (0-9)")
  ; )
  ; ((trim-ws (list string 0)))
; ))

; main loop
(tmpfn () (do
  (:= inp (ask "> "))
  (if (!= inp nil) (do
    (:= answer (calc-parse inp))
    (if (!= (head (scd answer)) "")
      (program.err (scd answer) "extraneous characters in input")
    )
    (:= answer (head answer))
    (pstr (repr answer))
    (pstr # \n)
    (rec)
  ))
) ())
