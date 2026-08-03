# Skylake Core library

- 100% specialized*, minimal c++ core utilities and abstractions library
- Has the absolutely minimal set of dependencies on any thirdparty
- Modular, designed to minimize header size for faster compilations

## Target platform, tools and libs
|    ~            | Name          | Version               |
|-----------------|---------------|-----------------------|
| <b>Compiler</b> | Clang         | 19+                   |
| <b>OS</b>       | Linux         | 6.1.0-31-amd64 and up |
| <b>Arch</b>     | x86_64        | 64                    |
| <b>Lang</b>     | C++           | 23+                   |
| <b>StdLib</b>   | Clang(libc++) | -                     |
| <b>CMake</b>    | CMake         | 4.0.0 and up          |

## Rational
Since we have lockedin the compiler, arch, os, c++ version,
we can drastically reduce our compilation times and implementation simplicity.
Eg. using compiler intrinsics directly in the implementation, including os headers directly etc

## Process model constraints

### `fork()` IS NOT SUPPORTED

**skylake-core does not support `fork()`. Do not fork a process that uses this library and then
continue to use the library in either the parent or the child.**

This is a deliberate design decision, not an oversight. Supporting fork means every stateful
thread local in the library needs a `pthread_atfork` handler or `MADV_WIPEONFORK` page, which
costs code, costs correctness surface, and buys nothing for a server that never forks. Rather
than half support it, the library does not support it at all.

The sharpest consequence is in `skl_secure_rand`: the CSPRNG keeps its AES key and counter in
ordinary thread local memory, so **a forked child inherits its parent's DRBG state and produces
the exact same byte stream**. Both sides would then hand out identical GUIDs, session ids and
tokens. Nothing detects this at runtime.

If a child process genuinely needs random bytes:
- `exec()` first - the new image seeds fresh, or
- use `skl::g_secure_random_bytes()` / `skl::g_make_guid()`, which hold no state and read the
  kernel CSPRNG on every call

The one tolerated exception is the fork-then-`abort()` pattern used to force a core dump, where
the child never calls back into the library before dying.

## Build
- cmake 
- llvm 19+
- python

<details>
  <summary><b>Ninja (Recommended)</b></summary>

    mkdir build
    cd build

    # Default
    cmake -G"Ninja" -S ../ -B . -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ 

    # Build
    ninja

</details>
<details>
  <summary><b>Make</b></summary>

    mkdir build
    cd build

    # Default
    cmake -G"Unix Makefiles" -S ../ -B . -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ 

    # Build
    make -j8

</details>

## Features/components/utilities
- See FEATURES.md

## Code statistics

Library sources only. Vendored dependencies (`third_party/`) and the test suite (`test/`) are
excluded.

| Language      |     Files |     Blank |   Comment |       Code |
|---------------|----------:|----------:|----------:|-----------:|
| C/C++ Header  |        86 |     2,440 |     3,038 |     10,214 |
| C++           |        30 |       757 |       435 |      3,390 |
| **Total**     |   **116** | **3,197** | **3,473** | **13,604** |

Comment density is 25.5% of code lines.

Public headers are extensionless by convention, so `cloc` cannot infer their language and must be
told explicitly. To reproduce:

```sh
cloc src/include --force-lang="C/C++ Header"
cloc src/source
```
