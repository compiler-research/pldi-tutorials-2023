# You can try this optimization pass by running build.sh after setting environment variables
#  LLVM_DIR and TUTORIAL_BUILD_DIR.
clang++ -emit-llvm -O3 -S .fft.cpp
$LLVM_DIR/bin/opt -load-pass-plugin=$TUTORIAL_BUILD_DIR/lib/p1-ex4.dylib -passes="modopt,dce" -S .fft.ll -o optimized.ll
clang++ optimized.ll -O3 -o fft