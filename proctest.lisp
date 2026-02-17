; run with: ./rho run-procedural.lisp proctest.lisp

(let name "World")

(fun fib (n)
  (if (< n 2)
    (return n)
    (return (+ (fib (- n 1)) (fib (- n 2))))
  )
)

(fun fib2 (n)
  (let a 0)
  (let b 1)
  (for (let i 0) (< i n) (:= i (+ i 1))
    (let tmp b)
    (:= b (+ a b))
    (:= a tmp)
  )
  (return a)
)

(fun main (args)
  (println (&$ "Hello, " name # !))
  (println "The 10th fibonacci number is:" (fib 10))
  (println "The 10th fibonacci number is:" (fib2 10))
  (return 0)
)
