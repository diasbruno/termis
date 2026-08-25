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

The core language has no mandatory standard library or runtime. Module paths are
loaded explicitly with `-I`/`--module-path`; each path contributes every
`.termis` file it contains.

```lisp
(module my/program
  (import std/data)

  (fn main () i64
    42))
```

```console
$ termisc -I std program.termis -o program
```

Modules wrap their declarations. Imports record module dependencies, but modules
are not bound to file boundaries and may be extended later; current conflict
detection is based on duplicate declarations.

The initial standard-library modules are intentionally small:

```text
std.data
std.memory
std.io
std.os
std.process
std.time
```

`std.data` defines `Data` as the conventional C-compatible byte/raw-memory
pointer:

```lisp
(type Data (& u8))
```

`Data` covers APIs that operate on C `void *` or `char *` style storage. It does
not imply length, ownership, encoding, or NUL termination.

`std.memory` uses `Data` and target-sized integers for allocation sizes:

```lisp
(extern fn allocate ((size usize)) Data "malloc")
(extern fn free ((data Data)) unit "free")
```

`Data` represents storage whose higher-level element type may be unknown. It is
not a dynamic `any` type and should be cast to a typed pointer before
dereferencing once casts and dereference operations are available.

Termis can declare C functions with `extern fn`. The optional final string names
the linked C symbol; without it, the Termis function name is used as the C symbol.

```lisp
(extern fn c-abs ((value i64)) i64 "llabs")

(fn main () i64
  (c-abs -42))
```

```console
$ make examples
```

The `examples` target builds every example program into `build/examples/`.

## Status

Termis is currently in the early design and bootstrap implementation stage.

The language syntax, type system, memory model, and compiler architecture are subject to change.

## License

Termis is released into the public domain under the Unlicense. See `UNLICENSE`.
