(def \* '((params body) T '(params T body)))
(def defm (\* (name params body)
  (def name (\* params body))
))

(defm \ (params body) '(params F body))
(defm defn (name params body)
  (def name (\ params body))
)

(defm tmpfn (params body args)
  (assoc (rec (\ params body))
    (eval (cons ' rec ' args))
  )
)
(defm defn-acc (name init params cond body) (eval (list ' defn ' name ' params (list ' tmpfn (cons ' acc ' params) '(if cond body acc) (cons init ' params)))))

(def -1 (- 1))

(defn not (x) (if x F T))
(defn < (a b) (not (+ 1 (cmp a b))))
(defn <= (a b) (< (cmp a b) 1))
(defn = (a b) (not (cmp a b)))
(defn > (a b) (not (- 1 (cmp a b))))
(defn >= (a b) (> (cmp a b) -1)) 
(defn != (a b) (truthy? (cmp a b))) 

(defm for (ix start stop body) (tmpfn (ix)
  (if (< ix stop) (do body (rec (+ ix 1))))
  (start)
))
(defm while (cond body)
  (if cond (do body (while cond body)))
)

(defn fold (acc fn lst)
  (if lst
    (fold (fn acc (head lst)) fn (tail lst))
    acc
  )
)
(defn map (fn lst)
  (if lst (cons (fn (head lst)) (map fn (tail lst))))
)

(defn-acc iota nil (n) n
  (rec (cons (- n 1) acc) (- n 1))
)
; (defn iota (n) (tmpfn (acc n)
  ; (if n (rec (cons (- n 1) acc) (- n 1)) acc)
  ; (nil n)
; ))

(defn-acc len 0 (l) l
  (rec (+ acc 1) (tail l))
)
(defn scd (l) (head (tail l)))

(defn print-val (val) (if (= (type val) ' string)
  (pstr val)
  (pstr (repr val))
))
(defn print (() args) (tmpfn (args)
  (if (and args (tail args))
    (do
      (print-val (head args))
      (pstr 32)
      (rec (tail args))
    )
    (do
      (if args (print-val (head args)))
      (pstr # \n)
      (if args (head args))
    )
  )
  (args)
))

(defm assert (what) (if what
  ()
  (do
    (print "assertion \"" ' what "\" failed.")
    (exit 1)
  )
))

(defn-acc * 0 (a b) b
  (rec
    (if (& b 1) (+ acc a) acc)
    (+ a a)
    (>> b 1)
  )
)
; (defn ! (n) (tmpfn (acc n) (if n (rec (* acc n) (- n 1)) acc) (1 n)))
(defn-acc ! 1 (n) n
  (rec (* acc n) (- n 1))
)
(defn fib (n) (tmpfn (a b n) (if n
  (rec b (+ a b) (- n 1))
  a
) (0 1 n)))
; (defn triag (n) (tmpfn (acc n) (if n (rec (+ acc n) (- n 1)) acc) (0 n)))
(defn-acc triag 0 (n) n
  (rec (+ acc n) (- n 1))
)
(defn-acc ** 1 (a b) b
  (rec
    (if (& b 1) (* acc a) acc)
    (* a a)
    (>> b 1)
  )
)

(defn parse-one (str)
  (scd (parse str))
)

(defm fopen (f () m) (if ' m (open ' f (head ' m)) (open ' f ' r)))
(defn prompt (prompt) (do
  (pstr prompt)
  (assoc (parsed (parse (readline stdin)))
    (if (tail parsed)
      (scd parsed)
      (write stderr "error: invalid input, expected a lisp value\n")
    )
  )
))
(defm with (name file body) (assoc (name file)
  (assoc (res body) (do
    (close name)
    res
  ))
))
(defn slurp (file) (with f (open file "r")
  (read f)
))

(defn run (prog) (assoc (parsed (parse prog))
  (if (tail parsed) (do
    (eval (scd parsed))
    (run (head parsed))
  ))
))
