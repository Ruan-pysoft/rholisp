(defm debug (x) (println ' x ' = x))

(defn-acc ! (n) "" 1 n (rec (* acc n) (- n 1)))
(defn fib (n) "" (tmpfn (a b n) (if n (rec b (+ a b) (- n 1)) a) (0 1 n)))
(defn-acc triag (n) "" 0 n (rec (+ acc n) (- n 1)))
