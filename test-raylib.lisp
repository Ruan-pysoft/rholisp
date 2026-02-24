(include "raylib.lisp")
(include "util.lisp")

; (InitWindow 800 600 "Hello World Window")
(println "Creating window...")
(def title "Test Window\0")
(InitWindow 800 600 (scd (!string-data-pointer title)))

(SetTargetFPS 24)

(tmpfn (r g b)
  (if (not (WindowShouldClose)) (do
    ; (println "Beginning drawing...")
    (BeginDrawing)

    ; (println "Clearing background...")
    (ClearBackground (Color r g b 255))

    (def text "Hello, world!\0")
    (DrawText
      (scd (!string-data-pointer text))
      (- 400 (/ (MeasureText (scd (!string-data-pointer text)) 32) 2))
      200
      32
      (Color (- 255 r) (- 255 g) (- 255 b) 255)
    )

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
