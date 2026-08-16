# Ocelotl Tensor Compiler

Ocelotl is an experimental tensor-oriented compiler written in modern C++20.

The project is designed as a systems/compiler engineering portfolio project focused on compiler architecture, semantic analysis, intermediate representations, tensor shape inference, and eventually LLVM-based code generation.

## Current compiler pipeline

```text
Source
  ↓
Lexer
  ↓
Parser
  ↓
AST
  ↓
Semantic Analysis
  ↓
Type & Shape Inference
  ↓
Ocelotl IR
  ↓
LLVM IR        ← in progress
  ↓
Native Code
```

## Current features

* C++20 codebase
* Lexer with source location and byte-offset tracking
* Recursive-descent parser
* Abstract Syntax Tree representation
* Parser diagnostics with source locations
* Tensor declarations
* Identifier resolution
* Semantic analysis
* Symbol table
* Duplicate declaration detection
* Type inference
* Tensor shape inference
* Built-in tensor operations

  * `matmul`
  * `relu`
* Custom SSA-inspired intermediate representation
* Integer and floating-point literal representation
* CMake + Ninja build system
* GoogleTest test suite
* GCC and Clang compatible codebase

## Example

```text
tensor A: f32[1024,512]
tensor B: f32[512,256]

C = matmul(A, B)
D = relu(C)

return D
```

Semantic analysis infers:

```text
A : f32[1024,512]
B : f32[512,256]
C : f32[1024,256]
D : f32[1024,256]
```

The current IR represents the computation as SSA-like values:

```text
%0 = tensor.decl A : f32[1024,512]
%1 = tensor.decl B : f32[512,256]
%2 = matmul %0, %1 : f32[1024,256]
%3 = relu %2 : f32[1024,256]
return %3
```

## Semantic validation

The compiler detects semantic errors before IR generation.

For example:

```text
tensor A: f32[1024,512]
tensor B: f32[128,256]

C = matmul(A, B)
```

is rejected because matrix multiplication requires compatible inner dimensions.

Conceptually:

```text
[M,K] × [K,N] → [M,N]
```

## Building

Requirements:

* C++20 compiler
* CMake
* Ninja
* GoogleTest

Configure:

```bash
cmake -S . -B build -G Ninja
```

Build:

```bash
cmake --build build
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

## Testing

The project currently includes tests covering:

* lexical analysis
* source locations
* malformed tokens
* parser behavior
* parser diagnostics
* AST construction
* symbol resolution
* semantic errors
* type inference
* shape inference
* IR generation

Current status:

```text
44 tests
44 passed
0 failed
```

## Architecture

```text
include/ocelotl/
├── ast/
├── frontend/
├── semantic/
└── ir/

src/
├── frontend/
├── semantic/
└── ir/

tests/
├── ast/
├── frontend/
├── semantic/
└── ir/
```

The compiler is intentionally separated into independent stages so that each layer can evolve without tightly coupling the frontend to a specific backend.

### Frontend

The frontend performs:

```text
source → tokens → AST
```

### Semantic analysis

Semantic analysis performs:

```text
AST
 ↓
symbol resolution
 ↓
type inference
 ↓
shape inference
 ↓
semantic validation
```

### Ocelotl IR

Ocelotl uses a custom intermediate representation between semantic analysis and backend lowering.

This separation allows the frontend language semantics to remain independent from LLVM-specific implementation details.

## Roadmap

### LLVM backend

Next milestone:

```text
Ocelotl IR
    ↓
LLVM lowering
    ↓
llvm::Module
    ↓
LLVM verification
    ↓
LLVM IR
    ↓
native object code
```

Planned work includes:

* LLVM type lowering
* LLVM IR generation
* `llvm::verifyModule`
* scalar code generation
* tensor representation
* native object generation
* LLVM optimization passes

### Toolchain engineering

Planned compiler/toolchain work:

* GCC and Clang CI matrix
* sanitizers
* compiler warnings as errors in CI
* Debian/Ubuntu packaging
* GCC plugin experiments
* LLVM optimization pipeline
* compiler diagnostics improvements

### Tensor compiler work

Longer-term areas of exploration:

* richer tensor operations
* broadcasting
* shape propagation
* constant folding
* custom optimization passes
* tensor lowering
* CPU code generation
* GPU-oriented lowering
* MLIR integration

## Project goals

Ocelotl is intended to explore practical compiler engineering rather than only language syntax.

Areas of focus include:

* compiler frontend design
* semantic correctness
* intermediate representation design
* SSA concepts
* optimization
* LLVM integration
* toolchain engineering
* CPU/GPU code generation
* testing and diagnostics

## License

License information will be added as the project matures.
