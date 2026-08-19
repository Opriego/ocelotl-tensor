# Ocelotl Tensor Compiler

Ocelotl is an experimental compiler written in modern C++20 for exploring end-to-end compiler engineering, from source-language analysis to native object generation.

The project currently implements a compiler frontend, semantic analysis, a custom SSA-inspired intermediate representation, an LLVM backend for scalar programs, automated testing, CI across GCC and Clang, and Debian packaging.

The longer-term goal is to evolve Ocelotl into a tensor-oriented compiler capable of progressively lowering tensor operations toward CPU and GPU execution.

---

## Compiler pipeline

```text
Ocelotl source
      |
      v
    Lexer
      |
      v
    Parser
      |
      v
     AST
      |
      v
Semantic Analysis
  - symbol resolution
  - type inference
  - shape inference
  - validation
      |
      v
   Ocelotl IR
  (SSA-inspired)
      |
      v
 LLVM Lowering
      |
      v
 llvm::Module
      |
      v
 LLVM Verifier
      |
      v
LLVM New Pass Manager
  -O0 / -O1 / -O2 / -O3
      |
      v
LLVM TargetMachine
      |
      +----> LLVM IR
      +----> assembly
      +----> relocatable object
```

The compiler deliberately keeps the frontend and semantic model independent from LLVM-specific implementation details.

This provides a clean boundary between language semantics, the Ocelotl intermediate representation, and backend lowering.

---

## Current status

### Frontend

Implemented:

* lexer with source locations and byte offsets
* recursive-descent parser
* Abstract Syntax Tree
* integer and floating-point literals
* variable assignments
* tensor declarations
* return statements
* parser diagnostics with source locations

### Semantic analysis

Implemented:

* symbol table
* identifier resolution
* duplicate declaration detection
* type inference
* tensor shape inference
* semantic validation
* validation of built-in tensor operations

Currently supported tensor operations include:

* `matmul`
* `relu`

For matrix multiplication, shape compatibility is validated before IR generation.

Conceptually:

```text
[M,K] × [K,N] -> [M,N]
```

---

## Ocelotl IR

Ocelotl uses a typed SSA intermediate representation between semantic analysis
and LLVM lowering. A module contains explicit basic blocks; each block contains
operations followed by exactly one branch, conditional branch, or return
terminator. Branch joins use phi nodes.

Example source:

```text
X = 42
return X
```

is represented conceptually as one entry block:

```text
entry:
  %0 = constant 42 : i64
  return %0
```

Tensor programs are also represented in Ocelotl IR.

For example:

```text
tensor A: f32[1024,512]
tensor B: f32[512,256]

C = matmul(A, B)
D = relu(C)

return D
```

produces an IR model equivalent to:

```text
%0 = tensor.decl A : f32[1024,512]
%1 = tensor.decl B : f32[512,256]
%2 = matmul %0, %1 : f32[1024,256]
%3 = relu %2 : f32[1024,256]
return %3
```

The IR verifier rejects missing terminators, invalid branch targets, nonexistent
SSA values, same-block use-before-definition, malformed phi predecessors, phi
type mismatches, and inconsistent return types. Full dominance analysis is not
yet implemented.

The custom IR is intentionally separate from LLVM IR so compiler analysis and tensor-specific transformations can evolve independently from the backend.

### Control flow through every layer

Source ([examples/control_flow.oc](examples/control_flow.oc)):

```text
X = 12
if X > 10 {
    Y = X + 1
} else {
    Y = X - 1
}
return Y
```

Conceptual AST:

```text
Program
├── Assign X = Integer(12)
├── If Binary(Greater, X, 10)
│   ├── then: Assign Y = Binary(Add, X, 1)
│   └── else: Assign Y = Binary(Subtract, X, 1)
└── Return Y
```

Custom IR:

```text
entry:
  %0 = constant 12 : i64
  %1 = constant 10 : i64
  %2 = compare greater %0, %1
  cond_br %2, if.then, if.else
if.then:
  %3 = constant 1 : i64
  %4 = add %0, %3 : i64
  br if.end
if.else:
  %5 = constant 1 : i64
  %6 = subtract %0, %5 : i64
  br if.end
if.end:
  %7 = phi i64 [if.then: %4], [if.else: %6]
  return %7
```

LLVM IR:

```llvm
define i64 @main() {
entry:
  %cmp = icmp sgt i64 12, 10
  br i1 %cmp, label %if.then, label %if.else
if.then:
  %add = add i64 12, 1
  br label %if.end
if.else:
  %sub = sub i64 12, 1
  br label %if.end
if.end:
  %merge = phi i64 [ %add, %if.then ], [ %sub, %if.else ]
  ret i64 %merge
}
```

Representative x86-64 assembly from LLVM's target backend:

```asm
main:
  xorl  %eax, %eax
  testb %al, %al
  jne   .LBB0_2
  movl  $13, %eax
  retq
.LBB0_2:
  movl  $11, %eax
  retq
```

---

## LLVM backend

Ocelotl contains a native LLVM-based code-generation layer under:

```text
src/codegen/llvm/
```

The current backend supports scalar CFG programs.

It currently lowers:

* integer constants to LLVM `i64`
* floating-point constants to LLVM `double`
* integer and floating-point arithmetic
* integer and floating-point comparisons
* basic blocks and conditional/unconditional branches
* Ocelotl SSA values and phi nodes
* return terminators

The generated module is checked with:

```cpp
llvm::verifyModule(...)
```

before being returned to the caller. It then runs LLVM's standard per-module
optimization pipeline through the New Pass Manager. Native emission finally
selects a target, assigns the target triple and target-provided `DataLayout`,
verifies again, and runs LLVM's target code-emission pipeline.

For example:

```text
X = 42
return X
```

can be compiled to LLVM IR, assembly, or a relocatable object with:

```bash
ocelotlc example.oc --emit-llvm -O2 -o example.ll
ocelotlc example.oc --emit-asm -O2 -o example.s
ocelotlc example.oc --emit-obj -O2 -o example.o
```

The native pipeline is:

```text
frontend -> Ocelotl IR -> LLVM IR -> LLVM optimization
         -> LLVM TargetMachine -> assembly/object
```

### Optimization layers

The compiler keeps three optimization domains distinct:

1. **Ocelotl IR optimization** would operate on Ocelotl-specific typed SSA and
   CFG operations. No Ocelotl optimization passes exist yet; the verified IR is
   currently lowered unchanged.
2. **LLVM IR optimization** uses `llvm::PassBuilder` and LLVM's standard
   per-module `-O0` through `-O3` pipelines. Ocelotl does not recreate or curate
   those pipelines manually.
3. **Target-machine optimization** happens during LLVM instruction selection,
   scheduling, register allocation, and assembly/object emission for the
   selected target.

The LLVM optimizer registers `LoopAnalysisManager`, `FunctionAnalysisManager`,
`CGSCCAnalysisManager`, and `ModuleAnalysisManager`, then cross-registers their
proxies before running the selected standard pipeline. The LLVM module is
verified immediately before and after optimization.

The optimization example can be inspected on both sides of the boundary:

```bash
ocelotlc examples/optimization.oc --emit-llvm-before-opt -O2 -o before.ll
ocelotlc examples/optimization.oc --emit-llvm -O2 -o after.ll
```

Before optimization, the function contains dead arithmetic, `add 0`, a constant
comparison, conditional branches, and a phi node. At `-O1` and above LLVM folds
the constants, removes the dead/redundant operations, and simplifies the CFG to:

```llvm
define noundef i64 @main() local_unnamed_addr {
entry:
  ret i64 13
}
```

The target defaults to the host. Cross-target configuration is explicit:

```bash
ocelotlc example.oc --emit-obj \
    --target=aarch64-unknown-linux-gnu \
    --cpu=generic \
    --features=+neon \
    -o example-aarch64.o
```

Available targets depend on the LLVM installation used to build Ocelotl.

### Current backend limitation

Tensor operations are represented by the frontend and Ocelotl IR, but LLVM lowering for:

* tensor declarations
* `matmul`
* `relu`

is not implemented yet.

Tensor lowering is the next major compiler-backend milestone.

---

## Command-line compiler

The compiler executable is:

```text
ocelotlc
```

Current modes include:

```bash
ocelotlc <source-file> --emit-tokens
```

and:

```bash
ocelotlc <source-file> --emit-llvm
```

```bash
ocelotlc <source-file> --emit-llvm-before-opt
```

```bash
ocelotlc <source-file> --emit-asm
ocelotlc <source-file> --emit-obj
```

`--emit-tokens` and `--emit-llvm` continue to write to standard output when
`-o` is omitted. Assembly and object modes default to the input basename with
`.s` and `.o`, respectively. All modes accept `-o <path>`.

Native emission accepts `--target=<triple>`, `--cpu=<cpu>`, and
`--features=<feature-string>`.

All LLVM and native emission modes accept `-O0`, `-O1`, `-O2`, or `-O3`.
The default is `-O0`. `--emit-llvm-before-opt` always shows the verified output
of LLVM lowering before the selected optimization pipeline; `--emit-llvm`
shows the post-optimization module.

Example:

```bash
./build/ocelotlc examples/return42.oc --emit-llvm
```

---

## Building

### Requirements

The project currently requires:

* C++20-capable GCC or Clang
* CMake 3.22+
* Ninja
* LLVM development libraries
* GoogleTest

On Debian or Ubuntu, the primary development dependencies can be installed with:

```bash
sudo apt update
sudo apt install cmake ninja-build llvm-dev libgtest-dev
```

### Configure

```bash
cmake -S . -B build -G Ninja
```

If CMake does not automatically locate LLVM:

```bash
cmake \
    -S . \
    -B build \
    -G Ninja \
    -DLLVM_DIR="$(llvm-config --cmakedir)"
```

### Build

```bash
cmake --build build
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

---

## Testing

The GoogleTest suite covers multiple compiler layers, including:

* lexical analysis
* source locations
* malformed tokens
* parsing
* parser diagnostics
* AST construction
* symbol resolution
* semantic errors
* type inference
* tensor shape inference
* Ocelotl IR generation
* custom IR structural and SSA verification
* arithmetic and comparison semantics
* true, false, and nested conditional paths
* CFG construction and phi generation
* LLVM IR generation
* LLVM module validity
* LLVM New Pass Manager analysis registration and standard pipelines
* constant folding, dead-code elimination, redundant-operation removal, and
  CFG simplification
* semantic equivalence at `-O0`, `-O1`, `-O2`, and `-O3`
* native linked execution and exit-value checks
* target triple and data-layout configuration
* host assembly emission
* relocatable host object emission and architecture validation
* invalid target diagnostics
* pre-emission LLVM module verification diagnostics
* unsupported backend operations

LLVM code-generation tests exercise the full path:

```text
source
  -> parser
  -> semantic analysis
  -> Ocelotl IR
  -> LLVM lowering
  -> llvm::Module
  -> LLVM verification
```

---

## Continuous integration

GitHub Actions builds and tests the project on Ubuntu using a compiler/build-mode matrix:

```text
GCC   / Debug
GCC   / Release
Clang / Debug
Clang / Release
```

The CI pipeline configures the project with CMake and Ninja and runs the compiler test suite for every supported matrix combination.

---

## Debian packaging

Ocelotl includes Debian package metadata under:

```text
debian/
```

including:

```text
debian/control
debian/rules
debian/changelog
debian/copyright
debian/ocelotlc.1
debian/ocelotlc.manpages
debian/source/
```

The binary package is named:

```text
ocelotlc
```

and uses the standard Debian CMake/Ninja build integration through `debhelper`.

A package can be built using standard Debian tooling, for example:

```bash
dpkg-buildpackage -us -uc -b
```

The package installs the compiler executable and its manual page.

---

## Repository structure

```text
.
├── .github/
│   └── workflows/
│       └── ci.yml
├── debian/
├── docs/
├── examples/
├── include/
│   └── ocelotl/
│       ├── ast/
│       ├── codegen/
│       ├── frontend/
│       ├── ir/
│       └── semantic/
├── src/
│   ├── codegen/
│   │   └── llvm/
│   ├── frontend/
│   ├── ir/
│   ├── semantic/
│   └── main.cpp
└── tests/
    ├── ast/
    ├── codegen/
    ├── frontend/
    ├── ir/
    └── semantic/
```

---

## Design goals

Ocelotl is intended to explore practical compiler engineering rather than only language parsing.

The project focuses on:

* frontend architecture
* compiler diagnostics
* semantic correctness
* symbol resolution
* type systems
* tensor shape analysis
* SSA concepts
* intermediate representation design
* LLVM integration
* compiler verification
* build-system integration
* toolchain testing
* Debian/Linux packaging
* eventual CPU and GPU code generation

---

## Roadmap

### Near term

* expand scalar LLVM lowering
* add arithmetic operations
* improve LLVM type lowering
* introduce control-flow representation
* implement basic blocks
* introduce PHI nodes where required
* extend compiler diagnostics
* expand LLVM backend tests
* add sanitizer CI configurations

### Tensor lowering

* define a concrete tensor memory representation
* lower tensor declarations
* lower element-wise operations
* lower `relu`
* lower matrix multiplication
* implement shape-aware transformations
* investigate loop-based CPU lowering

### Optimization

* constant folding
* dead-code elimination
* algebraic simplification
* data-flow analysis
* Ocelotl IR optimization passes
* LLVM optimization-pipeline integration

### Longer term

* native object generation
* CPU-oriented tensor optimizations
* SIMD/vector lowering
* GPU-oriented lowering
* MLIR experiments
* custom compiler passes

---

## Project scope

Ocelotl is an experimental compiler and compiler-engineering project.

It is not currently intended to be a production tensor framework or a replacement for established machine-learning compiler stacks.

The value of the project is in implementing and exposing the compiler pipeline explicitly, allowing individual stages to be studied, tested, extended, and optimized independently.

---

## License

Ocelotl Tensor Compiler is source-available under the PolyForm Noncommercial License 1.0.0.

The repository may be used, studied, modified, and redistributed for uses permitted by that license.

Commercial use requires a separate license from the copyright holder.

Copyright © 2026 Oscar Priego.
