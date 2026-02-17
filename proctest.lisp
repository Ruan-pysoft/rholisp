; run with: ./rho run-procedural.lisp proctest.lisp

(let name "World")

(fun fib (n)
  (if (< n 2)
    (return n)
    (return (+ (fib (- n 1)) (fib (- n 2))))
  )
)

(let MOD (+ (* 1000 1000 1000) 7))

(fun fib2 (n)
  (let a 0)
  (let b 1)
  (for (let i 0) (< i n) (++ i)
    (let tmp b)
    (:= b (% (+ a b) MOD))
    (:= a tmp)
  )
  (return a)
)

(let .fib-fast.memoized ())

(fun fib-fast (n)
  ; See "SAPO Final Round Practice Contest" > FIBONACCI at https://saco-evaluator.org.za/cms/
  (if (< n 2) (return n))
  (if (= n 2) (return 1))

  ; Working with the identity:
  ;   fib(2n+1) = fib(n+1)^2 + fib(n)^2
  ;   fib(2n)   = (fib(n-1) + fib(n+1))*fib(n)
  ; in order to compute large fibonacci numbers.

  ; (fib n) does O(n^2) operations, making it impractical for large values of n
  ;   in fact, I think the runtime is worse, because trying even (fib 50) should take very long...
  ; (fib2 n) does O(n) operations, allowing calculation of larger fibonacci numbers,
  ;    like (fib2 1000) in a reasonable amount of time
  ; (fib-fast n) does (roughly) O(log n) operations, meaning that
  ;    (fib-fast 10^18) ~= (fib-fast 2^60) can be done in O(60) operations
  ;    a quick test indicates that (fib-fast 10^18) takes about 213 operations,
  ;    which can be computed in a reasonable amount of time
  ;    (assuming 3 billion operations per second, (fib2 10^18) should take almost 32 years!)

  (for (let memoized .fib-fast.memoized) memoized (:= memoized (tail memoized))
    (let pair (head memoized))
    (if (= (head pair) n) (return (scd pair)))
  )

  (let half (fib-fast (/ n 2)))
  (let half-more (fib-fast (+ (/ n 2) 1)))
  ; (println "n:" n "; half:" half "; half-more" half-more)
  (let result ())
  (if (& n 1)
    ; odd
    (:= result (% (+ (* half half) (* half-more half-more)) MOD))
    ; even
    (:= result (% (* (+ half-more (fib-fast (- (/ n 2) 1))) half) MOD))
  )
  (:= .fib-fast.memoized (cons (list n result) .fib-fast.memoized))
  (return result)
)

(fun main (args)
  (println (&$ "Hello, " name # !))
  (println "The 10th fibonacci number is:" (fib 10))
  (println "The 10th fibonacci number is:" (fib2 10))
  (for (let i 0) (< i 100) (++ i)
    (println i (fib-fast i) ' == (fib2 i))
    ;(println .fib-fast.memoized)
  )
  (println "fib(1000000000) %" MOD "=" (fib-fast 1000000000))
  (println "fib(1000000000000000) %" MOD "=" (fib-fast 1000000000000000))
  (println "fib(10^18) %" MOD "=" (fib-fast (** 10 18)))
  (println "memoized" (len .fib-fast.memoized) "values.")
  (return 0)
)
