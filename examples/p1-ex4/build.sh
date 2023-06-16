# You can try this optimization pass by running build.sh after setting environment variables
#  LLVM_DIR and TUTORIAL_BUILD_DIR.
mkdir -p build
clang++ -O3 fft.cpp -o build/fft_unoptimized
clang++ -emit-llvm -O3 -S fft.cpp -o build/unoptimized.ll
clang++ -emit-llvm -O3 -c fast_modmul.cpp -o build/fast_modmul.bc
cd build && $LLVM_DIR/bin/opt -load-pass-plugin=$TUTORIAL_BUILD_DIR/lib/modopt.dylib \
  -load-pass-plugin=$TUTORIAL_BUILD_DIR/lib/fast_modmul_adder.dylib \
  -passes="modmuladder,function(modopt),dce" -S unoptimized.ll -o optimized.ll
clang++ optimized.ll -O3 -o fft_optimized