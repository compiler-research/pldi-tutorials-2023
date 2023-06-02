# Overview

LLVM and Clang are one of the foundational instruments for building research and
production programming languages. The libraries are efficient and composable
providing many opportunities to build research language infrastructure. The
tutorial is organized into three major parts:

  * Introduction, design principles, library layering (~45m) Provides an
    overview of LLVM and Clang.

  * Just-in-Time Infrastructure (~60m) Introduces new capabilities of ORCv2 such
    as speculative JITing and re-optimization using a simple language called
    Kaleidoscope.

  * Incremental compilation apt for dynamic programming languages (~45m)
    Outlines how to use Clang as a library to enable build basic C/C++/Python
    on-demand by building a C++ interpreter which connects to the Python
    interpreter.

Upon completion of the tutorials researchers learn how to set up various LLVM
components and use them to quickly bootstrap their research projects.

The tutorial will be given by Sunho Kim (De Anza College, Cupertino);
Lang Hames (Apple); Vassil Vassilev (Princeton/CERN).

# Build instructions

```bash
git clone https://github.com/compiler-research/pldi-tutorials-2023.git
cd pldi-tutorials-2023.git && mkdir build
cmake -DCMAKE_CXX_STANDARD=17 -DLLVM_DIR=/path/to/llvm/ ../
make
```
