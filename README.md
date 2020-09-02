# Lispy is a small dialect of Lisp written in C
It is based on the book [Build Your Own Lisp](http://buildyourownlisp.com) by Daniel Holden.
This project is being completed for a final project in [CS50x](http://cs50.harvard.edu/x/2020/).

# Syntax
Lispy50 uses Polish Notation, meaning the operator comes first and the operand(s) are second.
For instance: `(+ 1 2 6)` or `(* 2 5)`

Expressions are evaluated recursively, so: `(* (+ 2 3) (- 10 8))`

will evaluate to: `(* 5 2)`

and then: `10`

# Types
Lispy50 is strongly typed and contains the following types:
Type    | Syntax Examples   | Notes
--------|-------------------|---------
Integer | `1`, `567`, `-4`, `0`     | 
Decimal | `1.1`, `.1`, `-5.7`     | May omit digit preceding decimal point, but not after.
Boolean | `true`, `false`       | 
String  | `"Hello, world!"`   |
S-Expression | `(min 5 6)` <br /> `(6.6)`   | S-Expressions are automatically evaluated.<br /> They must begin with a function or operator or be a single value.
Q-Expressions | `{5 6 7}` <br /> `{cats dogs}` | Q-Expressions are space-separated lists and are not automatically evaluated. 
Functions | `(head {5 6 7})` <br /> `(last {"X" "Y" "Z"})` | 

## S-Expressions
An S-Expression is a collection of other expressions that can be evaluated down to
a single value.
An S-Expression may be empty: `()`,
contain a single value: `(5)`,
or contain a function and arguments: `(min 5 6 7)`, `(join "Hello, " "world!")`.

S-Expression may be nested inside one another and are evaluated from inner-most to outer-most expression: `(+ (min 6 5) (max (/ 4 2) (* 4 2)))`.

## Q-Expressions
Most Lisp dialects have a special form 'quote' that prevents evaluation of an expression.
In Lispy50, Q-Expressions are designated by curly braces: `{}`. A Q-Expression may contain any number of values, including other Q-Expressions or S-Expressions. A Q-Expression may be thought of as a list and can be consumed by certain functions as an argument: `(head {1 2 3 4})`

Q-Expressions may be created directly or can be created by passing elements to the `list` function: `(list 1 2 3 4)` returns `{1 2 3 4}`

A Q-Expression may be evaluated by passing it to the `eval` function: `(eval (head {(+ 1 2) (+ 10 20)})`.

# Variables
Variables can be defined using the `def` keyword followed by a Q-Expression with the symbol name and then the value to be stored to that variable.
Examples:
```
(def {x} 1.2)
(def {y} (+ 5 6))
(def {z} {1 2 3 4})
```
Variables are evaluated before the expression.

# Functions
Functions can be defined using the `lambda` keyword or the backslash symbol `\\` which is meant to be similar to the greek symbol lambda. The word or symbol is followed bye a Q-Expression with the symbol name(s) and another Q-Expression with the operations to be performed. Examples:
```
lambda {x y} {+ x y}
\ {x} {* x x}
```

You may call the function by putting it inside an S-Expression and following it with the appropriate values:
```
(\ {x y} {- x y}) 20 10
```
Functions may be stored by name using the `def` function:
```
def {add-together} (\ {x y} {+ x y})
```

Lispy50 also provides a simplified way of defining and storing functions using the `fun` function:
```
fun {add-together x y} {+ x y}
```



