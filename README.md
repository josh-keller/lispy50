# Lispy is a small dialect of Lisp written in C
It is based on the book [Build Your Own Lisp](http://buildyourownlisp.com) by Daniel Holden.
This project is being completed for a final project in [CS50x](http://cs50.harvard.edu/x/2020/).

# Syntax
Lispy50 uses Polish Notation, meaning the operator comes first and the operand(s) are second.
For instance: `(+ 1 2 6)` or `(* 2 5)`
Expressions are evaluated recursively, so:
`(* (+ 2 3) (- 10 8))`
will evaluate to:
`(* 5 2)`
and then: `10`

# Types
Lispy50 is strongly typed and contains the following types:
Type    | Syntax Examples   | Notes
--------|-------------------|---------
Integer | 1, 567, -4, 0     | 
Decimal | 1.1, .1, -5.7     | May omit digit preceding decimal point, but not after.
Boolean | true, false       | 
String  | "Hello, world!"   |
S-Expression | (min 5 6) <br /> (6.6)   | S-Expressions are automatically evaluated.<br /> They must begin with a function or operator or be a single value.
Q-Expressions | {5 6 7} <br /> {cats dogs} | Q-Expressions are space-separated lists and are not automatically evaluated. 
Functions | (head {5 6 7}) <br /> (last {"X" "Y" "Z"}) | 

## S-Expressions
An S-Expression is a collection of other expressions that can be evaluated down to
a single value.
An S-Expression may be empty: `()`,
contain a single value: `(5)`,
or contain a function and arguments: `(min 5 6 7)` `(join "Hello, " "world!")`
