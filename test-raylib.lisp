(include "raylib.lisp")
(include "util.lisp")

; (InitWindow 800 600 "Hello World Window")
(println "Creating window...")
(InitWindow 800 600 0)

(println "Beginning drawing...")
(BeginDrawing)

(println "Clearing background...")
(println "  with color:" (Color 170 51 34 255))
(debug (ClearBackground (Color 170 51 34 255)))

(println "Ending drawing...")
(EndDrawing)

(println "Sleeping...")
(debug (!ffi-call (!ffi-sym (!ffi-load "libc.so.6") "sleep") ' u32 ' u32 2))

(println "Closing window...")
(CloseWindow)
