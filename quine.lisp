; rholisp quine; running this program produces its own source
; test with: ./rho quine.lisp | diff - quine.lisp
; written by Ruan on Thursday, 12 February 2026
; during calculus class... oops... (I get distracted easily)

(def payload '("; rholisp quine; running this program produces its own source\n; test with: ./rho quine.lisp | diff - quine.lisp\n; written by Ruan on Thursday, 12 February 2026\n; during calculus class... oops... (I get distracted easily)\n\n(def payload '" () ")\n(defn disp (payload a () b)\n  (do\n    (if a (pstr a) (pstr (repr payload)))\n    (if b (call disp (cons payload b)))\n  )\n)\n(call disp (cons payload payload))\n"))
(defn disp (payload a () b)
  (do
    (if a (pstr a) (pstr (repr payload)))
    (if b (call disp (cons payload b)))
  )
)
(call disp (cons payload payload))
