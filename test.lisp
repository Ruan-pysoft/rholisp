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
