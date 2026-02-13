; run with: ./rho run-procedural.lisp proctest.lisp

(let name "World")

(fun fib (n)
  (if (< n 2)
    (return n)
    (return (+ (fib (- n 1)) (fib (- n 2))))
  )
)

(fun main (args)
  (println (&$ "Hello, " name # !))
  (println "The 10th fibonacci number is:" (fib 10))
  (return 0)
)
