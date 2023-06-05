/* See the LICENSE file in the project root for license terms. */

/// This file demonstrates how we could use C++ and CUDA.

#include "clang/Basic/Version.h"
#include "clang/Config/config.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"

#include "llvm/Support/ManagedStatic.h" // llvm_shutdown
#include "llvm/Support/TargetSelect.h"
llvm::ExitOnError ExitOnErr;

std::string MakeResourcesPath() {
  using namespace llvm;
#ifdef LLVM_BINARY_DIR
  StringRef Dir = LLVM_BINARY_DIR;
#else
  // Dir is bin/ or lib/, depending on where BinaryPath is.
  void *MainAddr = (void *)(intptr_t)MakeResourcesPath;
  std::string BinaryPath = llvm::sys::fs::getMainExecutable(/*Argv0=*/nullptr, MainAddr);

  // build/tools/clang/unittests/Interpreter/Executable -> build/
  StringRef Dir = sys::path::parent_path(BinaryPath);

  Dir = sys::path::parent_path(Dir);
  Dir = sys::path::parent_path(Dir);
  Dir = sys::path::parent_path(Dir);
  Dir = sys::path::parent_path(Dir);
  //Dir = sys::path::parent_path(Dir);
#endif // LLVM_BINARY_DIR
  SmallString<128> P(Dir);
  sys::path::append(P, CLANG_INSTALL_LIBDIR_BASENAME, "clang",
                    CLANG_VERSION_MAJOR_STRING);
  return P.str().str();
}

int main() {
  using namespace clang;
  llvm::llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  // Initialize all targets (required for device offloading)
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();

  clang::IncrementalCompilerBuilder CB;

  // Help find cuda's runtime headers.
  std::string ResourceDir = MakeResourcesPath();
  CB.SetCompilerArgs({"-resource-dir", ResourceDir.c_str(), "-std=c++20"});

  // Create the device compiler.
  std::unique_ptr<clang::CompilerInstance> DeviceCI;
  CB.SetOffloadArch("sm_35");
  DeviceCI = ExitOnErr(CB.CreateCudaDevice());

  std::unique_ptr<clang::CompilerInstance> CI;
  CI = ExitOnErr(CB.CreateCudaHost());

  auto Interp = ExitOnErr(Interpreter::createWithCUDA(std::move(CI),
                                                      std::move(DeviceCI)));
  ExitOnErr(Interp->LoadDynamicLibrary("libcudart.so"));

  std::string HostCode = R"(
    extern "C" int printf(const char*, ...);
    __host__ __device__ inline int sum(int a, int b){ return a + b; }
    __global__ void kernel(int * output){ *output = sum(40,2); }
    printf("Host sum: %d\n", sum(41,1));
  )";

  // Run on the host.
  ExitOnErr(Interp->ParseAndExecute(HostCode));

  std::string DeviceCode = R"(
    int var = 0;
    int * deviceVar;
    printf("cudaMalloc: %d\n", cudaMalloc((void **) &deviceVar, sizeof(int)));
    kernel<<<1,1>>>(deviceVar);
    printf("CUDA Error: %d\n", cudaGetLastError());
    printf("cudaMemcpy: %d\n", cudaMemcpy(&var, deviceVar, sizeof(int), cudaMemcpyDeviceToHost));
    printf("var: %d\n", var);
  )";

  // Run on the device.
  ExitOnErr(Interp->ParseAndExecute(DeviceCode));

  // That's all, folks!
}
