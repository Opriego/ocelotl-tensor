# Ocelotl Tensor Compiler

Ocelotl is an experimental compiler written in modern C++20 for exploring end-to-end compiler engineering, from source-language analysis to LLVM IR generation.

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
   LLVM IR
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

Ocelotl uses a custom SSA-inspired intermediate representation between semantic analysis and LLVM lowering.

Example source:

```text
X = 42
return X
```

is represented conceptually as:

```text
%0 = constant 42
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

The custom IR is intentionally separate from LLVM IR so compiler analysis and tensor-specific transformations can evolve independently from the backend.

---

## LLVM backend

Ocelotl contains a native LLVM-based code-generation layer under:

```text
src/codegen/llvm/
```

The current backend milestone supports scalar constant programs.

It currently lowers:

* integer constants to LLVM `i64`
* floating-point constants to LLVM `double`
* return operations
* Ocelotl SSA-like value IDs to `llvm::Value*`

The generated module is checked with:

```cpp
llvm::verifyModule(...)
```

before being returned to the caller.

For example:

```text
X = 42
return X
```

can be compiled with:

```bash
./build/ocelotlc examples/return42.oc --emit-llvm
```

and produces LLVM IR containing a generated `main` function returning the scalar value.

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
* LLVM IR generation
* LLVM module validity
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



