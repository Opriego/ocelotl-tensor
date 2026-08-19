<!--
Copyright (C) 2026 Oscar Priego Verdugo
SPDX-License-Identifier: GPL-3.0-only
-->

# Ocelotl runtime ABI v1

`libocelotlrt` is a small support library for operations that generated code
cannot or should not implement independently. It is separate from `ocelotlc`:
the compiler runs at build time, while the runtime is linked into or loaded by
the generated program on its target platform.

The installed, versioned C header is:

```c
#include <ocelotl/runtime/v1/runtime.h>
```

The `v1` path and symbol prefix are the ABI major version. The shared library
has SONAME `libocelotlrt.so.1`. Additive, backward-compatible changes may be
made within ABI v1; incompatible signatures or semantics require a v2 header,
symbol prefix, and SONAME.

## Exported functions

```c
void *ocelotl_rt_v1_alloc(uint64_t size, uint64_t alignment);
void ocelotl_rt_v1_free(void *pointer);
const char *ocelotl_rt_v1_last_error(void);
```

All functions use C linkage and default symbol visibility. All other runtime
symbols are hidden.

### `ocelotl_rt_v1_alloc`

- `size` is the requested number of bytes and must be nonzero.
- `alignment` must be a power of two and at least `sizeof(void *)` on the target.
- Success returns storage aligned to `alignment` and owned by the caller.
- Failure returns `NULL` and records a diagnostic for the calling thread.
- Storage is uninitialized.
- The result must be released exactly once with `ocelotl_rt_v1_free`.

### `ocelotl_rt_v1_free`

- Accepts a pointer returned by `ocelotl_rt_v1_alloc`, or `NULL`.
- Releases ownership. Using or freeing a non-NULL pointer afterward is invalid.
- Passing any other non-NULL pointer is invalid usage with undefined behavior.

### `ocelotl_rt_v1_last_error`

- Returns a non-NULL, borrowed pointer to a thread-local NUL-terminated string.
- An empty string means that the last allocation on this thread succeeded.
- The pointer remains owned by the runtime and must not be freed or modified.
- A later allocation on the same thread may change the reported diagnostic.

Allocation failure is recoverable at the ABI boundary; the runtime does not
abort and does not print implicitly. Generated tensor storage currently has
compiler-managed lexical lifetime: LLVM lowering calls `alloc`, keeps cleanup
slots in the function entry block, and calls `free` on every return path.

## Generated-program flow

```text
source tensor declaration
 -> Ocelotl TensorDeclOp
 -> LLVM external call to ocelotl_rt_v1_alloc
 -> target object with undefined runtime symbols
 -> system linker
 -> libocelotlrt.so.1 or libocelotlrt.a
 -> executable
```

Dynamic link example:

```bash
ocelotlc examples/runtime_allocation.oc --emit-obj -O2 -o runtime.o
c++ runtime.o -locelotlrt -o runtime-example
readelf -d runtime-example
```

Static runtime link example:

```bash
c++ runtime.o /usr/lib/libocelotlrt.a -o runtime-example-static
```

The static example links the Ocelotl runtime statically; it does not promise a
fully static system executable.
