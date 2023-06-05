/* See the LICENSE file in the project root for license terms. */

/// This file demonstrates how we could embed Clang and use it as a library in a
/// codebase.

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"

#include "llvm/Support/ManagedStatic.h" // llvm_shutdown
#include "llvm/Support/TargetSelect.h"

llvm::ExitOnError ExitOnErr;

int main() {
  using namespace clang;

  llvm::llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  // Allow low-level execution.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  // Initialize our builder class.
  clang::IncrementalCompilerBuilder CB;
  CB.SetCompilerArgs({"-std=c++20"});

  // Create the incremental compiler instance.
  std::unique_ptr<clang::CompilerInstance> CI;
  CI = ExitOnErr(CB.CreateCpp());

  // Create the interpreter instance.
  std::unique_ptr<Interpreter> Interp
      = ExitOnErr(Interpreter::create(std::move(CI)));

  // Parse and execute simple code.
  ExitOnErr(Interp->ParseAndExecute(R"(extern "C" int printf(const char*,...);
                                       printf("Hello Interpreter World!\n");
                                      )"));

  // Create a value to store the transport the execution result from the JIT.
  clang::Value V;
  ExitOnErr(Interp->ParseAndExecute(R"(extern "C" int square(int x){return x*x;}
                                       square(12)
                                      )", &V));
  printf("From JIT: square(12)=%d\n", V.getInt());

  // Or just get the function pointer and call it from compiled code:
  auto SymAddr = ExitOnErr(Interp->getSymbolAddress("square"));
  auto squarePtr = SymAddr.toPtr<int(*)(int)>();
  printf("From compiled code: square(13)=%d\n", squarePtr(13));

  // Can we instantiate templates on demand?
}
