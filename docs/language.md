<!--
Copyright (C) 2026 Oscar Priego Verdugo
SPDX-License-Identifier: GPL-3.0-only
-->

# Ocelotl Tensor Language

Ocelotl is a small compiler language for exploring frontend, IR, and native
backend engineering. Its scalar subset supports integer and floating-point
literals, immutable-style assignments, arithmetic, comparisons, structured
conditionals, and returns. Tensor syntax remains available, but tensor lowering
is intentionally limited: declarations allocate compiler-managed storage using
`libocelotlrt`, while tensor computations are not yet lowered.

## Scalar grammar

```text
program        := statement*
statement      := assignment | tensor-declaration | return | if-statement
assignment     := identifier "=" expression
return         := "return" expression
if-statement   := "if" expression block "else" block
block          := "{" statement* "}"
expression     := comparison
comparison     := additive (("==" | "!=" | "<" | "<=" | ">" | ">=") additive)*
additive       := multiplicative (("+" | "-") multiplicative)*
multiplicative := primary (("*" | "/") primary)*
primary        := identifier | integer | float | call | "(" expression ")"
```

`if` conditions must have scalar `i1` type, which is produced by a comparison.
Arithmetic operands must have the same scalar numeric type. Integer division is
signed; floating-point comparisons use ordered predicates except `!=`, which is
unordered-not-equal.

An `else` branch is mandatory. A name introduced inside a conditional becomes
available afterward only if every continuing branch defines it with the same
type. The IR generator represents differing branch definitions with an SSA phi
node.

## Control-flow example

```ocelotl
X = 12
if X > 10 {
    Y = X + 1
} else {
    Y = X - 1
}
return Y
```

Nested conditionals and direct returns from both branches are supported.

## Tensor example

```ocelotl
tensor A: f32[1024,512]
tensor B: f32[512,256]
C = matmul(A, B)
D = relu(C)
return D
```

Tensor declarations lower to runtime-backed storage and are released before
function return. Tensor operations are semantically checked and represented in
Ocelotl IR, but the LLVM backend deliberately rejects them until their lowering
is implemented.
