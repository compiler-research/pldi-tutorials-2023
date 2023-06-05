/* See the LICENSE file in the project root for license terms. */

/// This file demonstrates how we could embed create a simple C++ repl.

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"

#include "llvm/LineEditor/LineEditor.h"
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
  CB.SetCompilerArgs({"-std=c++20"}); // pass `-xc` for a C REPL.

  // Create the incremental compiler instance.
  std::unique_ptr<clang::CompilerInstance> CI;
  CI = ExitOnErr(CB.CreateCpp());

  // Create the interpreter instance.
  std::unique_ptr<Interpreter> Interp
      = ExitOnErr(Interpreter::create(std::move(CI)));

  llvm::LineEditor LE("pldi-cpp-repl");
  bool HadError = false;
  while (std::optional<std::string> Line = LE.readLine()) {
    if (*Line == "%quit")
      break;
    if (auto Err = Interp->ParseAndExecute(*Line)) {
      llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "error: ");
      HadError = true;
    }
  }

  return HadError;
}
