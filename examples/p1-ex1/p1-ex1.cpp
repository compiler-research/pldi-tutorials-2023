/* See the LICENSE file in the project root for license terms. */

#include "Kaleidoscope.h"

#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

using namespace llvm;
using namespace llvm::orc;

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  ExitOnError ExitOnErr("kaleidoscope: ");

  std::unique_ptr<KaleidoscopeJIT> J = ExitOnErr(KaleidoscopeJIT::Create());

  KaleidoscopeParser P;

  llvm::LineEditor LE("kaleidoscope");
  while (auto Line = LE.readLine()) {
    std::optional<KaleidoscopeParser::ParseResult> PR = P.parse(*Line);
    if (!PR)
      continue;

    // If the parser generated a function then CodeGen it to LLVM IR and add
    // it to the JIT.
    auto TSM = P.codegen(std::move(PR->Fn), J->DL);
    if (!TSM)
        continue;

    ExitOnErr(J->CompileLayer.add(J->MainJD, std::move(*TSM)));


    // If this wasn't a top-level expression then just continue.
    if (PR->TopLevelExpr.empty())
      continue;

    // If this was a top-level expression then look it up and run it.
    Expected<ExecutorSymbolDef> ExprSym =
      J->ES->lookup(&J->MainJD, J->Mangle(PR->TopLevelExpr));
    if (!ExprSym) {
      errs() << "Error: " << toString(ExprSym.takeError()) << "\n";
      continue;
    }

    double (*Expr)() = ExprSym->getAddress().toPtr<double (*)()>();
    double Result = Expr();
    outs() << "Result = " << Result << "\n";
  }

  return 0;
}
