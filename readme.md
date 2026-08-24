# Termis

Termis is a statically typed systems programming language using S-expressions.

It aims to combine a small and expressive syntax with predictable low-level memory semantics and a powerful compile-time environment.

## Features

* S-expression syntax
* Static typing
* C-like memory model
* Product, sum, and union types
* Pattern matching
* Parametric and phantom types
* Explicit pointers
* Compile-time evaluation
* Compile-time macros
* Predictable memory layout
* LLVM IR generation
* No mandatory garbage collector or runtime

## Example

```lisp
(type Option (T)
  (sum
    None
    (Some T)))

(fn unwrap-or ((value (Option i32))
               (fallback i32))
    i32
  (match value
    ((Some x) x)
    (None fallback)))

(fn noop () unit
  .)

(fn main () i32
  (unwrap-or (Some 42) 0))
```

## Types

Termis uses `type` as the common type declaration mechanism.

```lisp
(type Point
  (product
    (x f32)
    (y f32)))

(type Result (T E)
  (sum
    (Ok T)
    (Error E)))

(type Word
  (union
    (value u32)
    (bytes (array u8 4))))
```

Pointers use `&`:

```lisp
(& Point)
```

The unit type is `unit`, and its sole value is `.`:

```lisp
(fn noop () unit
  .)
```

## Compiler

The initial Termis compiler targets LLVM IR.

```text
Termis source
    ↓
S-expression reader
    ↓
macro expansion
    ↓
semantic analysis
    ↓
type checking
    ↓
typed IR
    ↓
lowering
    ↓
LLVM IR
```

The initial compiler executable is expected to be called `termisc`.

```console
$ termisc hello.termis -o hello.ll
```

## Status

Termis is currently in the early design and bootstrap implementation stage.

The language syntax, type system, memory model, and compiler architecture are subject to change.

## License

Termis is released into the public domain under the Unlicense. See `UNLICENSE`.
