(include util.lisp)

(debug (* 5 4))
(debug (! 5))
(debug (! 10))

(for i 0 16 (println (cons ' fib (list i)) ' = (fib i)))
(for i 0 16 (println (cons ' ! (list i)) ' = (! i)))
(for i 0 16 (println (cons ' triag (list i)) ' = (triag i)))

(def lst (iota 16))
(debug lst)
(debug (fold 0 + lst))
(debug (fold 1 * (tail lst)))

(debug (** 2 14))
(debug (** 3 10))

(defn fib-slow (n) (if (< n 2) n (+ (fib-slow (- n 1)) (fib-slow (- n 2)))))

(debug fib-slow)
(debug (fib-slow (do 30 20)))
(debug (fib-slow 10))
(debug (fib-slow 15))

(defn collatz (n) (if (= n 1) (println n) (if (& (println n) 1) (collatz (+ (* 3 n) 1)) (collatz (/ n 2)))))

(for i 1 32 (do
  (println (cons ' collatz (list i)) ' :)
  (collatz i)
))

(assert (= (** 2 10) 1024))
(assert (and (= (** 3 3) 25) '(this should fail)))
