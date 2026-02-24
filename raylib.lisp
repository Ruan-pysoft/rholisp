(def .raylib.lib (!ffi-load "./libraylib.so"))

(if (= (type .raylib.lib) ' string) (do
  (write stderr (&$ "Couldn't load raylib: " .raylib.lib # \n))
  (exit 1)
))

(def struct-Color '(u8 u8 u8 u8))
(defn Color (r g b a) (list r g b a))

(defm .raylib.define-func (fnname args rettype argtypes)
  (tmpfn (.fnname .args .rettype .argtypes)
    (assoc (
      .symname (scd (parse (&$ ".raylib." (repr .fnname) ".cfunc")))
    ) (do
      (call def (list .symname (list ' !ffi-sym ' .raylib.lib (repr .fnname))))
      (call assert (list (list ' != (list ' type .symname) ' ' string)))
      (call defn (list
        .fnname .args
        (tmpfn (acc .args .argtypes)
          (if .args
            (do
              (assert (!= .argtypes ()))
              (rec
                (append (append acc (head .argtypes)) (head .args))
                (tail .args)
                (tail .argtypes)
              )
            )
            (do
              (assert (= .argtypes ()))
              acc
            )
          )
          ((list ' !ffi-call .symname .rettype) .args .argtypes)
        )
      ))
    ))
    (' fnname ' args ' rettype ' argtypes)
  )
)

(.raylib.define-func InitWindow (width height window-name) () (' i32 ' i32 ' u64))
(.raylib.define-func CloseWindow () () ())
(.raylib.define-func BeginDrawing () () ())
(.raylib.define-func EndDrawing () () ())
(.raylib.define-func ClearBackground (color) () (struct-Color))
(.raylib.define-func SetTargetFPS (fps) () (' i32))
(.raylib.define-func WindowShouldClose () ' i32 ())
