// Copyright 2023 Vassil Vassilev
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


/// This file demonstrates how we could embed Clang and use it as a library in a
/// codebase.

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
  CB.SetCompilerArgs({"-std=c++20"});

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

  // Can we instantiate templates on demand?
}