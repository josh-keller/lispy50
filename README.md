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

# Builtins and Standard Library

Here are the functions built in to Lispy50. More functions and documentation to come.

## Basic Mathematical Functions
Function Name   | Syntax                        | Description
----------------|-------------------------------|-----------------------
add, +          | `(+ 5 5)`                     | Add two or more numbers together. Integer/Decimal.
sub, -          | `(- 6 3)` `(- 5)`             | Subtract one or more numbers from the first argument. Also unary negation.
mul, *          | `(* 5 5)`                     | Multiplication of two or more numbers.
div, /          | `(\ 2 2)` `(\ 4.5 1.5)`       | Division. Integers will return the integer portion. Decimal will return decimal.
mod, %          | `(% 6 5)`                     | Modulo. Returns the remainder when dividing two integers.
pow, ^          | `(pow 2 2)`                   | Power function. Raises first argument to power of second. Returns a decimal
min, max        | `(min 5 6 7)`                 | Returns the min or max value in the arguments.

## Conditional and Ordering Functions
Function Name   | Syntax                        | Description
----------------|-------------------------------|-----------------------
<, >, <=, >=    | `(< 5 6)`                     | Comparison operators
==, !=          | `(== 5 5)`                    | Equality operators
not, !          | `(! (== 4 5))`                | Logical negation
or, ||, and, && | `(and (== 5 5) (< 4 5))`      | Logical Or/And
if              | `(if (== 4 5) {...} {...})`   | If: evaluates to first Q-Expression when conditional is true, otherwise evaluates to second.

## List Functions
Function Name   | Syntax                        | Description
----------------|-------------------------------|-------------------------
list            | `(list 5 6 7 9)`                | Creates a list (Q-expr) from the arguments
head            | `(head {5 6 7 8})`              | Returns a list containing the first element of the list passed
tail            | `(tail {5 6 7 8})`              | Returns a list containing all but the first element of the list passed
eval            | `(eval {+ 1 2})`                | Evaluates a Q-Expression
join            | `(join {5 6} {7 8})` <br /> `(join "Hello " "world")` | Joins two or more lists or strings            
cons            | `(cons 5 {6 7})`                | Adds a value onto the beginning of a list
init            | `(init {6 7 8})`                | Returns list with all but the last element
len             | `(len {6 7 8})`, `(len "Hello")`  | Returns the number of elements in a list.


## Functions and Environment
Function Name   | Syntax                        | Description
----------------|-------------------------------|-------------------------
def             | `(def {x} 6)`                 | Assigns to variable. Symbol must be in Q-Expression.
=               | `(= {y} "hello")`             | Assigns to variable. Local scope in functions.
env             | `env`                         | Prints current environment variable
lambda, \\ <br /> fun | See [functions](#Functions)   | See [functions](#Functions)

## String and File Functions
Function Name   | Syntax                        | Description
----------------|-------------------------------|-------------------------
load            | (load "test.lspy")            | Loads a lispy file from disk.
print           | (print "Hello")               | Prints to output. Useful when loading a file at command line.


# Standard Library
More documentation on the standard library coming soon.
