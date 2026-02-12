; This program no longer works; it was created with a much earlier version of the interpreter
; one difference with that version, is that macro bodies were executed like function bodies, with the result being evaluated again
; (def \* (list (quote (_params _body)) T (quote (list _params T _body))))
(def \* '((_params _body) T (list _params T _body)))
(def \ (\* (_params _body) (list _params F _body)))
(def _while (\ (_cond _body) (if (eval _cond) (do (eval _body) (_while _cond _body)))))
(def while (\* (_cond _body) (_while _cond _body)))
(assoc (a 4 b 5 res 0) (do (while b (do (print (quote res:) (:= res (+ res a))) (print (quote b:) (:= b (- b 1))))) res))
(def fib (\ (n) (assoc (a 0 b 1 tmp ()) (do (while n (do (:= tmp (+ a b)) (:= a b) (:= b tmp) (:= n (- n 1)))) a))))
(fib 10)
(def * (\ (a b) (assoc (res 0) (do (while b (do (:= res (+ res a)) (:= b (- b 1)))) res))))
(* 6 7)
