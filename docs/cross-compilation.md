# Cross-compilation model

Ocelotl distinguishes three platforms:

- **Build platform**: the machine on which `ocelotlc` itself is compiled.
- **Host platform**: the machine on which the resulting `ocelotlc` executable
  runs.
- **Target platform**: the machine for which that `ocelotlc` invocation emits
  assembly or an object file.

In the normal development configuration, both build and host are x86-64 Linux.
Passing `--target=aarch64-linux-gnu` changes only the target platform.

This leads to two different cross-compilation problems:

1. **Cross-compiling programs with ocelotlc**: an x86-64 `ocelotlc` emits an
   AArch64 object. This is implemented.
2. **Cross-compiling ocelotlc itself**: a build machine produces an AArch64
   `ocelotlc` executable. This is not implemented by this milestone.

## Emitting AArch64 code

LLVM triples are normalized before target lookup, so this command:

```bash
ocelotlc foo.oc \
    --target=aarch64-linux-gnu \
    --cpu=generic \
    --emit-obj \
    -O2 \
    -o foo.o
```

configures the LLVM module with the canonical triple
`aarch64-unknown-linux-gnu` and the `DataLayout` supplied by the AArch64
`TargetMachine`.

Inspect the result without executing it:

```bash
llvm-readelf -h foo.o
# or
llvm-readobj --file-headers foo.o
```

The ELF header must report `EM_AARCH64`. The equivalent explicit X86-64 target
reports `EM_X86_64`.

Assembly emission needs no external assembler:

```bash
ocelotlc foo.oc \
    --target=aarch64-unknown-linux-gnu \
    --cpu=generic \
    --emit-asm \
    -O2 \
    -o foo.s
```

Both `.s` and `.o` are produced directly through LLVM target infrastructure;
`ocelotlc` does not invoke Clang or GCC for code generation.

## Target configuration and ABI boundary

- `--target` chooses the object format, architecture, OS/environment model, and
  default ABI conventions understood by LLVM.
- `--cpu` chooses a target CPU model. `generic` is suitable for portable
  baseline AArch64 output.
- `--features` adds or removes LLVM subtarget features.
- `DataLayout` always comes from the selected `TargetMachine`.
- Relocation model and code model currently use LLVM's target defaults because
  Ocelotl does not expose overrides yet.
- ABI selection currently comes from the normalized triple and LLVM defaults;
  there is no separate `--abi` option.

These settings are sufficient to emit a relocatable object. They do not provide
a linker, startup objects, C library, dynamic loader, sysroot, or runtime.

## Linking and running AArch64 output

If an AArch64 GNU userspace toolchain is installed, the object can be linked:

```bash
aarch64-linux-gnu-gcc foo.o -o foo
```

This normally requires an AArch64 sysroot containing startup objects, libc, and
the dynamic loader. On Debian/Ubuntu that sysroot is commonly rooted at
`/usr/aarch64-linux-gnu`.

If QEMU user-mode emulation is also installed:

```bash
qemu-aarch64 -L /usr/aarch64-linux-gnu ./foo
```

CMake detects `aarch64-linux-gnu-gcc` and `qemu-aarch64` independently. Object
generation and ELF inspection always remain testable when LLVM has the backend;
link/execution tests are enabled only when both optional tools are available.
Missing optional tools produce an explicit configure message rather than a test
failure or a silent runtime requirement.

## Cross-compiling ocelotlc itself

Building an AArch64 `ocelotlc` executable would require a CMake toolchain file
that sets at least `CMAKE_SYSTEM_NAME`, `CMAKE_SYSTEM_PROCESSOR`, the AArch64 C
and C++ compilers, and a sysroot. It would also require AArch64-host LLVM
development libraries and GoogleTest binaries; the x86-64 LLVM libraries used
by the build machine cannot be linked into an AArch64 executable.

That work is deliberately separate. It changes the host platform of
`ocelotlc`, whereas `--target` changes only the platform of programs produced
by an already-running compiler.
