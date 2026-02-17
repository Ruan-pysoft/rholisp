(def defm '(
  (name params () args)
  (defm "  name params body -> defines the macro with name `name`, which takes in parameters `params`, and has the body `body`\n  name params doc body -> the same as the previous form, but also includes the specified documentation")
  T
  (if (and ' args (if (tail ' args) F T))
    (def name (list
      ' params
      ' (name "")
      T
      (head ' args)
    ))
  (if (and ' args (tail ' args) (if (tail (tail ' args)) F T))
    (def name (list
      ' params
      (list ' name (head ' args))
      T
      (head (tail ' args))
    ))
  (do
    (write stderr "error in defm: expected either three or four arguments\n")
    (exit 1)
  )))
))
(defm \* (params body)
  "  -> the anonymous macro which takes parameters `params` and has body `body`"
  '(params T body)
)

(defm defn (name params () args)
  "  name params body -> defines the function with name `name`, which takes in parameters `params`, and has the body `body`\n  name params doc body -> the same as the previous form, but also includes the specified documentation"
  (if (and ' args (if (tail ' args) F T))
    (def name (list
      ' params
      ' (name "")
      F
      (head ' args)
    ))
  (if (and ' args (tail ' args) (if (tail (tail ' args)) F T))
    (def name (list
      ' params
      (list ' name (head ' args))
      F
      (head (tail ' args))
    ))
  (do
    (write stderr "error in defn: expected either three or four arguments\n")
    (exit 1)
  )))
)
(defm \ (params body)
  "  -> the anonymous function which takes parameters `params` and has body `body`"
  '(params F body)
)

(defm tmpfn (params body args)
  "  -> defines a temporary function `rec` taking parameters `params` with body `body`, and runs it with the arguments `args`"
  (assoc (rec (\ params body))
    (eval (cons ' rec ' args))
  )
)

(defm defn-acc (name params doc init cond body)
  "  -> defines the function `name` with docstring `doc`, taking parameters `params` and with an accumulator variable `acc` initially set to `init`, which runs the code `body` if the condition `cond` holds (allowing recursion using (rec)), or returns the accumulator otherwisw\n  example: (defn-acc ! (n) \"calculate the nth factorial\" 1 n (rec (* acc n) (- n 1)))"
  (eval (list ' defn ' name ' params ' doc (list ' tmpfn (cons ' acc ' params) '(if cond body acc) (cons init ' params))))
)

(defn not (x)
  "  -> boolean negation"
  (if x F T)
)
(defn < (a b)
  "  -> T if a is less than b, otherwise F"
  (not (+ 1 (cmp a b)))
)
(defn <= (a b)
  "  -> T if a is less than or equal to b, otherwise F"
  (< (cmp a b) 1)
)
(defn = (a b)
  "  -> T if a is equal to b, otherwise F"
  (and
    (not (cmp (type a) (type b))) ; types must be equal
    (not (cmp a b))
  )
)
(defn > (a b)
  "  -> T if a is greater than b, otherwise F"
  (not (- 1 (cmp a b)))
)
(defn >= (a b)
  "  -> T if a is greater than or equal to b, otherwise F"
  (> (cmp a b) (- 1))
)
(defn != (a b)
  "  -> T if a is unequal to b, otherwise F"
  (or
    (truthy? (cmp (type a) (type b))) ; differing types => inequality
    (truthy? (cmp a b))
  )
)

(defm for (ix start stop body)
  "  -> runs `body` with `ix` defined as each number in the range [`start`, `stop`) in turn"
  (tmpfn (ix)
    (if (< ix stop) (do body (rec (+ ix 1))))
    (start)
  )
)
(defm while (cond body)
  "  -> as long as `cond` evaluates to a truthy value, runs `body`"
  (if cond (do body (while cond body)))
)

(defn fold (acc fn lst)
  "  -> reduce `lst` down to a single value using the binary function `fn`, starting at the value `acc`"
  (if lst
    (fold (fn acc (head lst)) fn (tail lst))
    acc
  )
)
(defn map (fn lst)
  "  -> apply the unary function `fn` to each element of `lst`, giving the transformed list"
  (if lst (cons
    (fn (head lst))
    (map fn (tail lst))
  ))
)

(defn-acc iota (n)
  "  -> a list containing each integer in the range [0, 1)"
  nil n
  (rec (cons (- n 1) acc) (- n 1))
)

(defn-acc len (list)
  "  -> the lenth of the list"
  0 list
  (rec (+ acc 1) (tail list))
)

(defn scd (list)
  "  -> get the second element of a list"
  (head (tail list))
)

(def #sp ([]$ " " 0)) ; space character

(defn print-val (val)
  "prints a value; as-is for strings, or its representation for non-strings"
  (if (= (type val) ' string)
    (pstr val)
    (pstr (repr val))
  )
)
(defn println (() args)
  "prints space-seperated arguments, ended off with a newlone"
  (tmpfn (args)
    (if (and args (tail args)) (do ; (>= (len args) 2)
      (print-val (head args))
      (pstr #sp)
      (rec (tail args))
    ) (do                          ; (< (len args) 2)
      (if args (print-val (head args)))
      (pstr # \n)
      (if args (head args))
    ))
    (args)
  )
)

(defn help (callable)
  "  -> prints help text for the given function or macro"
  (if (if (cmp (type callable) ' builtin) F T)
    ; is a builtin
    (do
      (pstr "builtin ")
      (pstr (if (:macro? callable) "macro " "function "))
      (print-val (:name callable))
      (pstr ":\n")
      (pstr (if (:docs callable) (:docs callable) "(no documentation provided)"))
      (pstr # \n)
    )
    ; is a user-defined function
    (if (:name callable)
      (do
        (pstr "user-defined ")
        (pstr (if (:macro? callable) "macro " "function "))
        (print-val (cons (:name callable) (head callable)))
        (pstr ":\n")
        (pstr (if (:docs callable) (:docs callable) "(no documentation provided)"))
        (pstr # \n)
      )
      (do (if (:macro? callable)
        (println
          "anonymous macro"
          (list ' \* (head callable) ' ...)
        )
        (println
          "anonymous function"
          (list ' \ (head callable) ' ...)
        )
      ) ())
    )
  )
)

(defm assert (what)
  (if what () (do
    (println (&$ "assertion \"" (repr ' what) "\" failed."))
    (exit 1)
  ))
)

(defm defn-binexp (name doc op init)
  "  -> define a binary function as binary exponentiation over some binary operation"
  (defn-acc name (a b)
    doc
    init b
    (rec
      (if (& b 1) (op acc a) acc)
      (op a a)
      (>> b 1)
    )
  )
)

(defn-binexp *
  "  -> a multiplied by b"
  + 0
)
(defn-binexp **
  "  -> a to the power of b"
  * 1
)

(defn parse-one (str)
  "  -> parse a single value from a string"
  (scd (parse str))
)

(defm fopen (file () mode)
  "  file -> opens the specified file in read mode, allowing a plain symbol for filenames\n  file mode -> opens the file in the given mode, also allowing a plain symbol for the mode specifier"
  (tmpfn (f m)
    (if (and m (tail m)) ; (>= (len m) 2)
      (do
        (write stderr "error in open: expected either one or two arguments\n")
        (exit 1)
      )
      (assoc (asstr (\ (x)
        (if (= (type x) ' string) x
        (if (and (= (type x) ' list) x) (eval x)
        (repr x)))
      )) (open
        (asstr f)
        (if m (asstr (head m)) "r")
      ))
    )
    (' file ' mode)
  )
)
(defm with (name file body)
  "  -> execute `body`, with `file` associates with `name`, closing `file` afterwards\n  example: (with f (fopen std.lisp) (read f)) ; gives the content of std.lisp"
  (assoc (name file)
    (assoc (res body) (do
      (close name)
      res
    ))
  )
)
(defn slurp (file)
  "  -> the entire contents of the specified file"
  (with f (open file "r") (read f))
)

(defn run (prog)
  "  -> parse and evaluate the program, given as a string"
  (assoc (parsed (parse prog))
    (if (tail parsed) (do
      (eval (scd parsed))
      (run (head parsed))
    ))
  )
)

(defm include (file)
  "  -> includes a lisp file"
  (with module (fopen file)
    (run (read module))
  )
)

(defm switch (on () branches)
  "  -> switch statement ; use (case val body) to match a value, and (default body) for a default branch"
  ((\ (.switch.res) (if .switch.res (head .switch.res)))
    (assoc (
        .switch.on on
        case (\* (of then)
          (and (= .switch.on of) (list then))
        )
        default (\* (then) (list then))
      )
      (call or (append ' branches nil))
    )
  )
)

(defm += (var val) (:= var (+ var val)))
(defm -= (var val) (:= var (- var val)))
(defm *= (var val) (:= var (* var val)))
(defm /= (var val) (:= var (/ var val)))
(defm %= (var val) (:= var (% var val)))
(defm |= (var val) (:= var (| var val)))
(defm &= (var val) (:= var (& var val)))
(defm ^= (var val) (:= var (^ var val)))
(defm or= (var val) (:= var (or var val)))
(defm and= (var val) (:= var (and var val)))
(defm &$= (var val) (:= var (&$ var val)))

(defm ++ (var) (+= var 1))
(defm -- (var) (-= var 1))
