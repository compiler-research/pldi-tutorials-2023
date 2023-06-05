/* See the LICENSE file in the project root for license terms. */

#include "Kaleidoscope.h"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ExecutionEngine/Orc/EPCDynamicLibrarySearchGenerator.h"
#include "llvm/ExecutionEngine/Orc/EPCIndirectionUtils.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

#include <limits>
#include <math.h>

using namespace llvm;
using namespace llvm::orc;

class KaleidoscopeASTMU : public MaterializationUnit {
public:
  KaleidoscopeASTMU(KaleidoscopeParser &P, KaleidoscopeJIT &J,
                    std::unique_ptr<FunctionAST> Fn)
    : MaterializationUnit(getInterface(J, *Fn)), P(P), J(J), Fn(std::move(Fn)) {}

  StringRef getName() const override {
    return "KaleidoscopeASTMU";
  }

  void materialize(std::unique_ptr<MaterializationResponsibility> R) override {
    if (auto TSM = P.codegen(std::move(Fn), J.DL))
      J.CompileLayer.emit(std::move(R), std::move(*TSM));
    else
      R->failMaterialization();
  }

private:

  static MaterializationUnit::Interface
  getInterface(KaleidoscopeJIT &J, FunctionAST &Fn) {
    SymbolFlagsMap Symbols;
    Symbols[J.Mangle(Fn.getName())] =
        JITSymbolFlags::Exported | JITSymbolFlags::Callable;
    return { std::move(Symbols) , nullptr };
  }

  void discard(const JITDylib &JD, const SymbolStringPtr &Sym) override {
    llvm_unreachable("Kaleidoscope functions are not overridable");
  }

  KaleidoscopeParser &P;
  KaleidoscopeJIT &J;
  std::unique_ptr<FunctionAST> Fn;
};

double handleLazyCompileFailure() {
  return std::numeric_limits<double>::signaling_NaN();
}

Expected<JITDylib&> makeProcessSymsJD(KaleidoscopeJIT &J) {
  // First we create a "generator" -- an object that can be attached to a
  // JITDylib to generate new definitions in response to lookups.
  auto G = EPCDynamicLibrarySearchGenerator::GetForTargetProcess(*J.ES);
  if (!G)
    return G.takeError();

  // If we're able to create the generator then we create a bare JITDylib
  // (we don't need any runtime functions in this JITDylib, since it's only
  // going to reflect existing symbols from outside the JIT), add the
  // generator, and return the new JITDylib so that MainJD can be linked
  // against it.
  auto &ProcessSymbolsJD = J.ES->createBareJITDylib("<Process_Symbols>");
  ProcessSymbolsJD.addGenerator(std::move(*G));
  return ProcessSymbolsJD;
}

extern "C" double circleArea(double radius) {
  return M_PI * radius * radius;
}

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  ExitOnError ExitOnErr("kaleidoscope: ");

  std::unique_ptr<KaleidoscopeJIT> J = ExitOnErr(KaleidoscopeJIT::Create());
  J->MainJD.addToLinkOrder(ExitOnErr(makeProcessSymsJD(*J)));

  KaleidoscopeParser P;

  auto EPCIU =
    ExitOnErr(EPCIndirectionUtils::Create(J->ES->getExecutorProcessControl()));
  auto EPCIUCleanup = make_scope_exit([&]() {
    if (auto Err = EPCIU->cleanup())
      J->ES->reportError(std::move(Err));
  });

  auto &LCTM = EPCIU->createLazyCallThroughManager(
      *J->ES, ExecutorAddr::fromPtr(&handleLazyCompileFailure));
  ExitOnErr(setUpInProcessLCTMReentryViaEPCIU(*EPCIU));
  auto ISM = EPCIU->createIndirectStubsManager();

  llvm::LineEditor LE("kaleidoscope");
  while (auto Line = LE.readLine()) {
    std::optional<KaleidoscopeParser::ParseResult> PR = P.parse(*Line);
    if (!PR)
      continue;

    // If the parser generated a function <func-name> then
    //   (1) rename the function in the AST to <func-name>$impl
    //   (2) add a lazy-reexport from <func-name> to <func-name>$impl
    //   (3) add the AST to the JIT
    std::string FnImplName = PR->Fn->getName() + "$impl";
    PR->Fn->setName(std::move(FnImplName));

    SymbolAliasMap ReExports({
        { J->Mangle(PR->Fn->getName()),
          { J->Mangle(FnImplName),
            JITSymbolFlags::Exported | JITSymbolFlags::Callable }
        }
      });
    ExitOnErr(J->MainJD.define(
          lazyReexports(LCTM, *ISM, J->MainJD, std::move(ReExports))));

    ExitOnErr(J->MainJD.define(
        std::make_unique<KaleidoscopeASTMU>(P, *J, std::move(PR->Fn))));

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
