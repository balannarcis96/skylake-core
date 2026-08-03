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

This is a deliberate design decision, not an oversight. Supporting fork would mean auditing and
fixing every item below, then keeping them fixed forever. That buys nothing for a server that
never forks, so rather than half support it the library does not support it at all.

The umbrella rule from POSIX already applies: after `fork()` in a multithreaded process, the
child may only call async-signal-safe functions until it `exec()`s. Practically nothing in this
library qualifies.

#### Why it does not currently work

Every one of these is silent - nothing detects any of it at runtime.

**1. The CSPRNG replays its stream.** `skl_secure_rand` keeps the AES key and counter in ordinary
thread local memory (`skl_secure_rand.cpp`). A child inherits both and produces the *exact same
byte stream* as its parent, so both hand out identical GUIDs, session ids and tokens. This is the
sharpest one.

**2. `SklRand` replays its stream.** Same story for the non-cryptographic RNG (`skl_rand.cpp`) -
the 8 byte state is inherited verbatim, so parent and child roll identical values forever.

**3. Thread ids stop being unique.** `skl_core_thread_id_t` (`skl_thread_id.cpp`) draws its id
from a fresh `SklRand` at thread init. The child inherits the forking thread's id, so two live
processes now claim the same "unique" thread id.

**4. Thread local singletons of dead threads leak and dangle.** Only the forking thread survives
into the child, but every other thread's TLS singleton block is still allocated (`skl_tls`, any
type above the small buffer optimization threshold). No thread exists to run `tls_destroy()` on
them, so they are unreachable and permanently leaked.

**5. The report registry points at those dead blocks.** `g_report_buffers` (`skl_report.cpp`) is a
process-global `skl_fixed_vector` of per-thread report buffer pointers. In the child it is still
fully populated, now referring to threads that do not exist.

**6. Logging interleaves across two processes.** The serialized logger has per-thread front end
buffers (`skl_slogger_fend.cpp`) while the sink is a process-global pointer array
(`skl_slogger_sink.cpp`) writing to a `FILE*` (`skl_slogger_sink_handle.cpp`). Parent and child
end up sharing one file descriptor with two independent userspace buffers - output interleaves,
duplicates, or tears.

**7. The huge page pools are duplicated wholesale.** `skl_huge_pages.cpp` maps with
`MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE`. `MAP_PRIVATE` means copy on write, so
the child instantly acquires its own copy of the entire pre-populated pool, and both processes
then believe they own it with the same free lists.

**8. `BufferPool` / `HugePageBufferPool` inherit a corrupt view.** Both are process-global and
constructed once in `skl_core_init()` (`skl_core.cpp`). The child inherits their allocation state
including every block currently handed out to threads that no longer exist.

**9. The child cannot re-initialize.** `g_is_skl_core_init` and `g_is_skl_core_init_on_thread`
(`skl_core.cpp`) are both already true in the child, so `skl_core_init()` returns
`SKL_OK_REDUNDANT` and refuses to rebuild anything - while the per-thread state of every other
thread is gone.

**10. Exit handlers fire twice.** `skl_signal.cpp` keeps global exit / abnormal exit / termination
/ core dump handler vectors, and `g_program_epilog_init` is already set in the child. The child
will run the parent's registered handlers on its own exit.

**11. The allocator has per-thread heaps.** Builds default to mimalloc (`SKL_CORE_USE_MIMALLOC=1`),
which keeps thread local heaps; the child inherits metadata for threads that do not exist.

**12. No thread survives.** `SKLThread` is pthread based (`skl_thread.cpp`). Threads are not
duplicated by `fork()`, so any inherited handle is dead and calling `join()` on it is undefined.

#### If a child genuinely needs random bytes

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
