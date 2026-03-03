(def l '(1 2 3))
(append l 4)
(def args '((1 2 3) 4))
(call append args)
(def args2 args)
(call append args2)

(append-to l 4)
(println l)
(def l2 l)
(append-to l 5)
(println l)
(println l2)

(exit)

(pstr "Hello, world!\n")

(def \* '((params body) T '(params T body)))
(def defm (\* (name params body) (def name (\* params body))))
(defm \ (params body) '(params F body))
(defm defn (name params body) (def name (\ params body)))

(defn fib-slow (n)
  (if (+ (cmp n 2) 1) (+ (fib-slow (- n 1)) (fib-slow (- n 2))) n)
)

(pstr "The 10th fibonacci number is: ")
(pstr (repr (fib-slow 10)))
(pstr # \n)
