(def .curses.lib (!ffi-load "libncursesw.so"))

(if (= (type .curses.lib) ' string) (do
  (write stderr (&$ "Couldn't load ncurses: " .curses.lib # \n))
  (exit 1)
))

(def int ' i32)
(def i64 ' i64)
(def u64 ' u64)
(def void nil)
(def ptr ' ptr)

(defn nc.call (name ret () args)
  (call !ffi-call (cons
    (!ffi-sym .curses.lib name)
    (cons ret args)
  ))
)

(defn !ffi-var (lib name)
  (list ' pointer (scd (!ffi-sym lib name)))
)
(defn read-var (type var)
  (head (!destruct-val var type))
)

(def stdscr (!ffi-var .curses.lib "stdscr"))

;(def stdscr (scd (!ffi-sym .curses.lib "stdscr")))

;(println (!destruct-val (!ffi-sym .curses.lib "COLORS") int))

(nc.call "initscr" void)
(nc.call "noecho" void)
(nc.call "cbreak" void) ; or "raw"?
(nc.call "keypad" void u64 (scd (read-var ptr stdscr)) int 1)
(nc.call "curs_set" void int 1)

(if (not (nc.call "has_colors" int)) (do
  (nc.call "endwin" void)
  (write stderr "Your terminal doesn't support colours.\n")
  (exit 1)
))

(nc.call "start_color" void)
(nc.call "init_pair" void int 1
  int 7 ; COLOR_WHITE
  int 4 ; COLOR_BLUE
)
(nc.call "attron" void int (nc.call "COLOR_PAIR" int int 1))

(def x (nc.call "getmaxx" int ptr (read-var ptr stdscr)))
(def y (nc.call "getmaxy" int ptr (read-var ptr stdscr)))

(def fmt "%d %d\0")
(nc.call "printw" void u64 (scd (!string-data-pointer fmt)) int y int x)
(println x y)

(:= y (/ y 2))
(:= x (- (/ x 2) 6))

(println x y)

(def text "Hello World!\0")
(nc.call "mvwprintw" int ptr (read-var ptr stdscr) int y int x u64 (scd (!string-data-pointer text)))
(nc.call "mvwprintw" int ptr (read-var ptr stdscr) int 2 int 2 u64 (scd (!string-data-pointer text)))

(nc.call "refresh" void)

(nc.call "attroff" void int (nc.call "COLOR_PAIR" int int 1))

(nc.call "getch" int)

(nc.call "endwin" void)
