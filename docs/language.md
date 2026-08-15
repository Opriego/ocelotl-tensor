# Ocelotl Tensor Language

Ocelotl Tensor is a small tensor-oriented DSL designed to explore compiler
frontends, semantic analysis, intermediate representations, optimization
passes, and GPU lowering.

The initial language intentionally has a small surface area.

## Example

```ocelotl
tensor A: f32[1024,1024]
tensor B: f32[1024,1024]
tensor bias: f32[1024]

C = matmul(A, B)
D = add(C, bias)
E = relu(D)

return E
