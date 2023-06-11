/* See the LICENSE file in the project root for license terms. */

#include "Kaleidoscope.h"

#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

using namespace llvm;
using namespace llvm::orc;

class KaleidoscopeASTMU : public MaterializationUnit {
public:
  KaleidoscopeASTMU(KaleidoscopeParser &P, KaleidoscopeJIT &J,
                    std::unique_ptr<FunctionAST> FnAST)
    : MaterializationUnit(getInterface(J, *FnAST)),
      P(P), J(J), FnAST(std::move(FnAST)) {}

  StringRef getName() const override {
    return "KaleidoscopeASTMU";
  }

  void materialize(std::unique_ptr<MaterializationResponsibility> R) override {
    // dbgs() << "Compiling " << FnAST->getName() << "\n";
    if (auto IRMod = P.codegen(std::move(FnAST), J.DL))
      J.CompileLayer.emit(std::move(R), std::move(*IRMod));
    else
      R->failMaterialization();
  }

private:

  static MaterializationUnit::Interface
  getInterface(KaleidoscopeJIT &J, FunctionAST &FnAST) {
    SymbolFlagsMap Symbols;
    Symbols[J.Mangle(FnAST.getName())] =
        JITSymbolFlags::Exported | JITSymbolFlags::Callable;
    return { std::move(Symbols), nullptr };
  }

  void discard(const JITDylib &JD, const SymbolStringPtr &Sym) override {
    llvm_unreachable("Kaleidoscope functions are not overridable");
  }

  KaleidoscopeParser &P;
  KaleidoscopeJIT &J;
  std::unique_ptr<FunctionAST> FnAST;
};

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
    auto ParseResult = P.parse(*Line);
    if (!ParseResult)
      continue;

    // If the parser generated a function then add the AST to the JIT.
    ExitOnErr(J->MainJD.define(
        std::make_unique<KaleidoscopeASTMU>(P, *J,
                                            std::move(ParseResult->FnAST))));

    // If this wasn't a top-level expression then just continue.
    if (ParseResult->TopLevelExpr.empty())
      continue;

    // If this was a top-level expression then look it up and run it.
    Expected<ExecutorSymbolDef> ExprSym =
      J->ES->lookup(&J->MainJD, J->Mangle(ParseResult->TopLevelExpr));
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
