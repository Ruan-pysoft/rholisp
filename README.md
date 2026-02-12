# <img src="https://raw.githubusercontent.com/Ruan-pysoft/rholisp/refs/heads/master/logo/rholisp.png" height="28px" /> rholisp

rholisp (or ρlisp) is a small lisp implementation in C.

The initial implementation was written entirely on my phone. You may access it at [rholisp.zip](./rholisp.zip), including the README I typed out on my phone, etc.

## Compiling & Running

There are two ways to run this program. You may either compile it first into an executable, then run said executable, or you may run it directly with `tcc`.

To compile and run, most C compilers should work, assuming the relevant headers are available (which I believe should be the case on Linux, BSD, and MacOS). You just need to feed the `rholisp.c` file to your compiler.

Note however, that I have thus far only tested it on Termux with gcc.

```
$ gcc -Wall -Wextra -Wno-override-init rholisp.c -o rho
$ # add `-g -Og` for better debugging/valgrind reports, or `-O2` for a faster interpreter
$ ./rho -h
rholisp interpreter.

Usage:
./rho [options]
    run interactive repl
./rho [options] -- [args]
    run interactive repl with command line arguments
./rho [options] <file> [args]
    run rholisp script file

Options:
    --help, -help, -h    print this help text and exit
    -nostd               do not load the standard library
                         before the script/repl
    -preload <file>      run the specified file before
                         the script/repl
```

If you have tcc available, you can run the interpreter directly:

```
$ tcc -run rholisp.c -h
```

You can try a simple hello world program as follows:

```
$ ./rho
> (println "Hello, world!")
Hello, world!
()
> 
```

You can press ctrl+D at any point to exit the repl.

To get help on any function or macro, use the `help` function:

```
$ ./rho
$ (help help)
user-defined function (help callable):
  -> prints help text for the given function or macro
()
> (help quote)
builtin macro quote:
  val -> returns the value as-is
  used to represent values literally rather than evaluating them
()
> 
```

Some example programs are included.

`script.lisp` just tests various language features and standard library functions. I have probably spent hours running it with valgrind to track down memory leaks.

`bf.lisp` is a [brainf\*\*\*](https://esolangs.org/wiki/Brainfuck) interpreter. This demonstrates that rholisp is [Turing](https://esolangs.org/wiki/Turing-complete) [complete](https://en.wikipedia.org/wiki/Turing_completeness). The brainf\*\*\* programs I used to test the interpreter is not included, as I do not own the copyright for them (and I can't be bothered with figuring out how to release them under a different license in the same repo...), but there are examples available on the esolangs.org page.

Features of `bf.lisp`:
 - the tape extends infinitely to the left and to the right
 - cells are 64-bit signed integers
 - EOF returns -1 on input

`peano.lisp` is just me messing around a bit.

`program.lisp` is one of the earliest programs, probably written within 24 hours of me starting to write the interpreter.

`quine.lisp` is a [Quine](https://esolangs.org/wiki/Quine) (see also the [Wikipedia page](https://en.wikipedia.org/wiki/Quine_%28computing%29)), a program that prints its own source code. I'm quite proud of it, as I wrote it entirely by myself; if you notice it resembles the procedure described on the esolangs.org page quite closely, that's because I first learnt how to write quines from that page a couple years ago (or perhaps a userpage on esolangs.org? For some reason I feel like it was a userpage)

`std.lisp` is the standard library implementation. It includes:
 - Named and anonymous macros (via `defm` and `\*`)
 - Named and anonymous functions (via `defn` and `\`)
 - Temporary functions for recursion (`tmpfn`)
 - Helpers for comparisons and boolean operators
 - Loops
 - A space character (`#sp`)
 - `println`, to print space-separated values of any type on a line
 - A `help` function to query function/macro documentation
 - `assert`
 - Multiplication and power of implemented with [binary exponentiation](https://cp-algorithms.com/algebra/binary-exp.html)
 - Some file utilities
 - `include`, to include libraries
   - note that it is more like c-like inclusion than proper importing; I'm sure I can work _something_ out, but it seems importing would require some sort of namespaces, and I really can't be bothered to figure out how to implement those, never mind on a phone screen and adding functions for interacting with them to the list of builtins
 - A `switch` structure for (very, very basic) pattern matching (okay, it's more like an if-else with automatic equality checking... still, I'm quite proud of it)

`std.old.lisp` is a previous version of the standard library, from before named functions/macros were implemented.

`test.lisp` is the file where I'd debug memory leaks and the like.

`tm.lisp` is a truth machine: run the program, and type in 0 (remember to press enter), and it will print 0 in response before exiting. Enter 1, and it will continue printing 1 to the console in an infinite loop.

`util.lisp` is a small library containing some utility functions.

## Language design

rholisp makes use of immutable values, and while it supports nested environments, it does not support closures.

It uses reference counting for memory management. As far as I can tell, there are no memory leaks (though valgrind reports a 56 byte leak when calling `exit` with an argument, though I suspect that valgrind is incorrect in this case; and the program ends immediately afterwards regardless, so a single, predictable leak right before program termination doesn't really matter)

As far as possible, I try to keep the builtins defined in C to a minimum, and instead implement useful functions and macros in the `std.lisp` file, which is automatically run before the program file or the prompt, unless the `-nostd` option is passed to the interpreter.

I make extensive use of both `assert` and `fprintf` + `exit(1)` for error reporting/detection. I know this is bad practice (and asserts in particular should document program assumptions, rather than being used for error detecting/reporting), but I'm typing this on a phone screen, so I really cannot be bothered to implement proper error reporting and handling.

The following datatypes are supported:
 - builtin: a built-in function or macro (defined in C)
 - list: a singly-linked list of values
 - number: a signed integer number
 - boolean: a boolean (T or F)
 - symbol: an identifier (roughly)
 - string: a string

The syntax is as follows:
 - a program consists of any number of values, one after another
 - ; starts a comment, which extends till the end of the line
 - a number consists of any number of numeric digits (0-9) right next to one another
   - eg: `42`, `13`, `007`, etc
 - a symbol consists of any number of non-whitespace characters directly next to one another, except for `(`, `)`, `"`, or `;`, as long as the sequence doesn't start with a digit
   - eg: `x`, `foo`, `-1`, `:id`, `+`, `@`, `&$`, `\*`
   - the symbols `T` and `F` resolve to the boolean true and false values, respectively
   - the `'` symbol must be followed by another value, and expands to `(quote <value>)`
     - eg: `' a` -> `(quote a)`, `' (+ 1 2)` -> `(quote (+ 1 2))`
   - the `#` symbol must be followed by another symbol, which is interpreted as a character, which results in a number equal to that character's ascii code
     - eg: `# a` -> 97, `# \n` -> 10
   - NOTE: because they are handled specially on a syntax level, the symbols `T`, `F`, `'`, and `#` should be unobtainable in a rholisp program
 - a string consists of some text between two double quotes `"`, with some escape sequences supported (`\n` for newline, `\"` to include a quote character, `\\` for a backslash, etc)
   - eg: `"Hello, world!"`, `""`, `"and we can do\t\"this\"!\n"`
 - a list consists of an opening parenthesis `(`, any number of values, and then a closing parenthesis `)`
   - eg: `()`, `(+ 1 1)`, `("hi!")`, `(a b c d 10e)`

### Types of environment

Environments exist as a stack, with the root environment at the bottom and the current environment at the top.

An environment consists of a list of symbol-value associations, with later ones shadowing earlier bindings. When looking up a symbol in the current environment, if the symbol is not found the next environment down will be searched. If the root environment is searched and yields no association, then the symbol is undefined, and will print an error to stderr and yield nil.

Environments exist as one of two types:
- A full environment allows the creation of new symbol-value bindings using `def`
 - A parameter environment does _not_ allow the creation of new symbol-value bindings using `def`; `def` will instead create a new binding in the first full environment down
The root environment is a full environment. Parameter environments are created on function calls and by `assoc`; `env-new` creates an empty full environment at the top of the environment stack.

### Evaluation

Most values evaluate to themselves, with the exception of symbols and non-empty lists.

A symbols will look up the value it corresponds to, erroring if it is not associated with any value. New symbols can be bound with `(def sym val)`, existing bindings can be modified with `(:= sym val)` (in both full and parameter environments), symbols can be bound into a temporary environment with `(assoc (sym1 val1 sym2 val2 ...) body)`, and a new sub-environment can be created with `(env-new body)`.

As an exception, `_` evaluates instead to the result of the previous value in a program or the repl.

The empty list (`()`, also bound to the symbol `nil` in the root environment) evaluates to itself.

When a non-empty list is evaluated, if first evaluates its first element. If that element is not callable (a builtin, or a list of a certain format; can be tested with `(:callable? ...)`), then the interpreter errors out.

If it _is_ a callable, evaluation proceeds differently for functions and macros (testable with `(:macro? ...)`):
 - for a macro, the rest of the list is passed as-is as arguments
 - for a function, each element in the rest of the list is first evaluated, then the resulting list of evaluated arguments is passed to the function

#### Functions and macros

A user-defined function or macro is a list of one of the following two formats:
 - `(list boolean value)`: anonymous function or macro
 - `(list (symbol string) boolean value)`: named function or macro

In either format, the first element is the parameter list, the boolean indicates whether it is a function or a macro (`T` for a macro, `F` for a function), and the last parameter is its body.

For named functions or macros, the second element is a pair of a symbol, its name, and a string, its documentation.

The parameter list is a (potentially empty) list of symbols, the last of which may be preceded by `()`. These are the parameter names. If the last parameter is preceded by `()`, then the function or macro is variadic, with all extra arguments (potentially zero of them) being bound to the last parameter as a list.

When a function is called, its evaluated arguments are bound to the associated parameter names in a new parameter environment, and its body is evaluated in this environment.

When a macro is called, its unevaluated arguments are assigned to the associated parameter names – note that no new environment is created – and the body is iterated only, with any instance of a parameter name being replaced with the assigned argument. One this substitution phase is complete, this new body is then evaluated in place of the original macro call.

As an example, take the function `(def dblf '((n) F (+ n n)))` and the macro `(def dblm '((n) T (+ n n)))`.

The function application `(dblf (do (pstr "working...") 1))` will first evaluate its argument, `(do (pstr "working...") 1)`, which prints `working...` to stdout and then yields one. Then it will execute the equivalent to the following: `(assoc (n 1) (+ n n))` resulting in `2`.

The macro application `(dblm (do (pstr "working...") 1))` will instead replace all instances of `n` in its body with its argument as-is, giving `(+ (do (pstr "working...") 1) (do (pstr "working...") 1))`, which will then be evaluated, evaluating both its arguments in turn, printing `working...working...` to stdout before resulting in `2`.

## Features

 - Functions and macros
 - User-defined functions and macros
 - [Turing complete](./bf.lisp)
 - A [reference-counting](https://en.wikipedia.org/wiki/Reference_counting) [garbage collector](https://en.wikipedia.org/wiki/Garbage_collection_%28computer_science%29) (hopefully without any bugs... I've spent _so many_ hours debugging the most _stupid_ bugs where I forget to free a value somewhere...)
 - Only constant values (this is totally a feature and not just because mutable values seemed like a pain in the a$$ to worl with)
 - Tail-call optimisation for both functions and macros, including many built-in macros like `if`, `or`, or `do`
   - I also avoid creating unnecessary parameter environments on direct tail-calls of user-defined functions (this one actually took a day or two to get working...)
 - [Dynamic scope](https://en.wikipedia.org/wiki/Scope_%28computer_programming%29) (once again, definitely an important feature included for important reasons, and not because I have previous experience of static scope and closures being a pain in the backside when it comes to implementing garbage collection with reference counting)
 - File handling
 - Insane speeds of up to 6x slower than Python!
   - I'm actually impressed, I timed script.lisp five times and script.py five times, and got an avarage of 163.2ms and 26.4ms on my phone; a few days ago I got something like rholisp being 40x slower than Python
 - ...and more! (probably)

## Future plans

Once I have the initial version uploaded, I plan on implementing some QoL things on an actual computer keyboard, such as better error reporting & handling, or support for floats.

I may fork the repo and create a dialect that departs more severely from the original version.

## Development tools

I used the following tools for developing rholisp:
 - [My phone](https://en.wikipedia.org/wiki/Samsung_Galaxy_S10)
 - [Termux](https://termux.dev/en/) + [Bash](https://en.wikipedia.org/wiki/Bash_%28Unix_shell%29)
 - [Neovim](https://neovim.io/)
 - [gcc](https://en.wikipedia.org/wiki/GNU_Compiler_Collection)
 - [tcc](https://en.wikipedia.org/wiki/Tiny_C_Compiler)
 - [valgrind](https://valgrind.org/)

Besides this, I also used pen and paper (I might upload photos of my scribbles along with this when I upload the code to Github), yapping to friends and family who didn't have a clue what's going on (I don't know any programming language enthusiasts irl ;-;), and some internet.

All the text included in the initial commit of this repo was typed by me on my on-screen keyboard, except some links I copy-pasted (okay, I also copy-pasted some lines of code, but I already typed them out at least once, so that doesn't count!), and the copyright notice text (copied from the unlicense website, both the text at the end of this file and in the `COPYING` file).

I must say, typing thousands of lines of code on a phone screen over the coarse of a few weeks is torture (I swear I'm not a masochist!), but now I have a better understanding of how it feels to program when you can't comfortably type at 80+ wpm. If that's you, get that typing speed up, it's worth it, trust me. (I can type at about 40 to 60 wpm on my phone)

## Copying

The logo ([rholisp.aseprite](./logo/rholisp.aseprite) and [rholisp.png](./logo/rholisp.png)) is released under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)<img src="https://mirrors.creativecommons.org/presskit/icons/cc.svg" height="14px" /><img src="https://mirrors.creativecommons.org/presskit/icons/zero.svg" height="14px" />. It was created by me using [Aseprite](https://www.aseprite.org/).

All code in this repo is released into the public domain, as per the [Unlicense](https://unlicense.org).

This was written by me as boredom relief and some light entertainment. I do not intend, expect, or desire to extract any sort of monetary or commercial gain from this product, and I am completely aware that I am waiving all rights I have over this code under copyright law by releasing it into the public domain.

I ask only, exclusively as a matter of courtesy, rather than as any sort of legal requirement, that I be credited as the author when this code or the programs described by this code is distributed, used, or demonstrated, in a manner and time you find appropriate.

The text of the Unlicense is as follows:

```
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>
```
