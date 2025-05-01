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

## Cloc
**2250 lines of C++ code**

|Language      | files | blank | comment | code |
|--------------|-------|-------|---------|------|
| C++          | 25    | 498   | 259     | 2002 |
| C/C++ Header | 4     | 61    | 70      | 248  |
| SUM:         | 29    | 550   | 329     | 2250 |
