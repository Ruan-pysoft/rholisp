(include "raylib.lisp")
(include "util.lisp")

; (InitWindow 800 600 "Hello World Window")
(println "Creating window...")
(InitWindow 800 600 0)

(SetTargetFPS 24)

(tmpfn (r g b)
  (if (not (WindowShouldClose)) (do
    ; (println "Beginning drawing...")
    (BeginDrawing)

    ; (println "Clearing background...")
    (ClearBackground (Color r g b 255))

    ; (println "Ending drawing...")
    (EndDrawing)

    (rec
      (% (+ r 1) 256)
      (% (+ g 3) 256)
      (% (+ b 5) 256)
    )
  ))
  (170 51 34)
)

(println "Closing window...")
(CloseWindow)
