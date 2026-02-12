; memory vm:
; (position (memory...))
(defn mem-create ()
  "  -> create the memory for the bf vm"
  '(0 (0))
)
(defn mem-next (memory)
  "  -> the memory value with the position moved forward by one"
  ;(do (println "going to next position in memory:" memory)
  (if (= (head memory) (- (len (scd memory)) 1))
    (list (+ (head memory) 1) (append (scd memory) 0))
    (list (+ (head memory) 1) (scd memory))
  )
  ;)
)
(defn mem-prev (memory)
  "  -> the memory value with the position moved backward by one"
  ;(do (println "going to previous position in memory:" memory)
  (if (= (head memory) 0)
    (list 0 (cons 0 (scd memory)))
    (list (- (head memory) 1) (scd memory))
  )
  ;)
)
(defn join (a b)
  "  -> join two lists"
  (if b (join (append a (head b)) (tail b)) a)
)
(defn mem-inc (memory)
  "  -> the memory with the value at the given location incremented by one"
  ;(do (println "incrementing memory:" memory)
  (tmpfn (pos-acc mem-acc pos mem)
    (if (= pos 0)
      (list pos-acc (join
        mem-acc
        (cons (+ (head mem) 1) (tail mem))
      ))
      (rec
        (+ pos-acc 1)
        (append mem-acc (head mem))
        (- pos 1)
        (tail mem)
      )
    )
    (0 () (head memory) (scd memory))
  )
  ;)
)
(defn mem-dec (memory)
  "  -> the memory with the value at the given location decremented by one"
  ;(do (println "decrementing memory:" memory)
  (tmpfn (pos-acc mem-acc pos mem)
    (if (= pos 0)
      (list pos-acc (join
        mem-acc
        (cons (- (head mem) 1) (tail mem))
      ))
      (rec
        (+ pos-acc 1)
        (append mem-acc (head mem))
        (- pos 1)
        (tail mem)
      )
    )
    (0 () (head memory) (scd memory))
  )
  ;)
)
(defn mem-out (memory)
  "  -> prints the value in memory at the given location as a character"
  (do
    (tmpfn (pos mem)
      (if (= pos 0)
        (pstr (& (head mem) 255))
        (rec (- pos 1) (tail mem))
      )
      ((head memory) (scd memory))
    )
    memory
  )
)
(def .line nil)
(defn getc ()
  "  -> gets a character from stdin"
  (if (= (type .line) ' string)
    (if (= .line "")
      (do (:= .line nil) # \n)
      (assoc (ch ([]$ .line 0)) (do
        (:= .line ([]$ .line 1 (len$ .line)))
        ch
      ))
    )
    (if (= (:= .line (readline stdin)) nil)
      (- 1)
      (getc)
    )
  )
)
(defn mem-inp (memory)
  "  -> reads a character from stdin and stores it at the specified position in memory"
  (tmpfn (pos-acc mem-acc pos mem)
    (if (= pos 0)
      (list pos-acc (join
        mem-acc
        (cons (getc) (tail mem))
      ))
      (rec
        (+ pos-acc 1)
        (append mem-acc (head mem))
        (- pos 1)
        (tail mem)
      )
    )
    (0 () (head memory) (scd memory))
  )
)

(def vm (mem-create))
(def prog (if (and (= (len args) 2) (!= (scd args) "-h") (!= (scd args) "--help"))
  (slurp (scd args))
  (if (= (len args) 1)
    (readline stdin)
    (do
      (write stderr (&$
        "Usage:" # \n
        "./rho bf.lisp" # \n
        "  reads bf program from first line of stdin" # \n
        "./rho bf.lisp <file>" # \n
        "  reads bf program from the given file" # \n
        "Try ./rho bf.lisp bf/hello_world.bf" # \n
      ))
      (exit)
    )
  )
))
(def pc 0)

(defn at-cmd? ()
  "  -> T if the pc is at a valid bf command, F otherwise"
  (and (< pc (len$ prog)) (assoc (ch ([]$ prog pc)) (or
    (= ch # <)
    (= ch # >)
    (= ch # -)
    (= ch # +)
    (= ch # ,)
    (= ch # .)
    (= ch # [)
    (= ch # ])
  )))
)
(defn find-cmd ()
  "  -> advance pc until it points to a bf command"
  ;(do (println "looking for command at" pc)
  (if (and (< pc (len$ prog)) (not (at-cmd?)))
    (do (:= pc (+ pc 1)) (find-cmd))
  )
  ;)
)
(defn loop-begin ()
  (if (= (nth (scd vm) (head vm)) 0)
    (do (:= pc (+ pc 1)) (tmpfn (depth)
      (if depth
        (if (>= pc (len$ prog))
          (assert (and "expected matching ]" F))
          (do
            (:= pc (+ pc 1))
            (rec
              (if (>= pc (len$ prog)) depth
              (if (= ([]$ prog pc) # [) (+ depth 1)
              (if (= ([]$ prog pc) # ]) (- depth 1)
              depth)))
            )
          )
        )
      )
      (1)
    ))
    (:= pc (+ pc 1))
  )
)
(defn loop-end ()
  (if (!= (nth (scd vm) (head vm)) 0)
    (do (:= pc (- pc 1)) (tmpfn (depth)
      (if depth
        (if (< pc 0)
          (assert (and "expected matching [" F))
          (do
            (:= pc (- pc 1))
            (rec
              (if (< pc 0) depth
              (if (= ([]$ prog pc) # [) (+ depth 1)
              (if (= ([]$ prog pc) # ]) (- depth 1)
              depth)))
            )
          )
        )
      )
      ((- 1))
    ))
    (:= pc (+ pc 1))
  )
)

(defn run-cmd (cmd)
  "  -> runs the command specifies by the given character"
  ; (do (println "pos:" pc "; mem:" vm)
  (switch cmd
    (case # < (do (:= vm (mem-prev vm)) (:= pc (+ pc 1))))
    (case # > (do (:= vm (mem-next vm)) (:= pc (+ pc 1))))
    (case # - (do (:= vm (mem-dec vm)) (:= pc (+ pc 1))))
    (case # + (do (:= vm (mem-inc vm)) (:= pc (+ pc 1))))
    (case # , (do (:= vm (mem-inp vm)) (:= pc (+ pc 1))))
    (case # . (do (:= vm (mem-out vm)) (:= pc (+ pc 1))))
    (case # [ (loop-begin))
    (case # ] (loop-end))
  )
  ; )
)

(defn run ()
  (if (do (find-cmd) (< pc (len$ prog)))
    (do (run-cmd ([]$ prog pc)) (run))
  )
)

(run)
