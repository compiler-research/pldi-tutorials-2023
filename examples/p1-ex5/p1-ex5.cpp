/* See the LICENSE file in the project root for license terms. */

#include "Kaleidoscope.h"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ExecutionEngine/Orc/EPCDynamicLibrarySearchGenerator.h"
#include "llvm/ExecutionEngine/Orc/EPCIndirectionUtils.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

#include <limits>

using namespace llvm;
using namespace llvm::orc;
using namespace llvm::jitlink;

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

double handleLazyCompileFailure() {
  return std::numeric_limits<double>::signaling_NaN();
}

extern "C" double circleArea(double radius) {
  return M_PI * radius * radius;
}

class MyPlugin : public ObjectLinkingLayer::Plugin {
public:
  void modifyPassConfig(MaterializationResponsibility &MR, LinkGraph &G,
                        PassConfiguration &PassConfig) override {
    PassConfig.PostAllocationPasses.push_back([this](LinkGraph &G) {
      return printGraph(G);
    });
  }

  Error notifyFailed(MaterializationResponsibility &MR) override {
    return Error::success();
  }
  Error notifyRemovingResources(JITDylib &JD, ResourceKey K) override {
    return Error::success();
  }
  void notifyTransferringResources(JITDylib &JD, ResourceKey DstKey,
                                   ResourceKey SrcKey) override {}

private:
  Error printGraph(LinkGraph &G) {
    // Print graph name:
    outs() << "Graph " << G.getName() << "\n";

    // Loop over sections
    for (auto &Sec : G.sections()) {
      outs() << "  Section " << Sec.getName() << ", "
             << Sec.getMemProt() << "\n";

      // Print section symbols
      outs() << "  Symbols:\n";
      for (auto *Sym : Sec.symbols())
        if (Sym->hasName())
          outs() << "    " << Sym->getAddress() << ": "
                 << Sym->getName() << "\n";
      outs() << "  Blocks:\n";

      // Print section blocks and edges.
      for (auto *B : Sec.blocks()) {
        outs() << "    " << B->getAddress() << ": ";
        if (B->isZeroFill())
          outs() << "zero-fill";
        else {
          outs() << "content = { ";
          for (size_t I = 0; I != std::min(B->getSize(), size_t(10)); ++I)
            outs() << formatv("{0:x2}", (uint8_t)B->getContent()[I]) << " ";
          if (B->getSize() > 10)
            outs() << "... ";
          outs() << "}";
        }
        outs() << ", " << B->getSize() << "-bytes, "
               << B->edges_size() << " edges\n";
        for (auto &E : B->edges()) {
          outs() << "      offset " << E.getOffset() << ": "
                 << G.getEdgeKindName(E.getKind()) << " edge to ";
          if (E.getTarget().hasName())
            outs() << E.getTarget().getName();
          else
            outs() << "<anonymous target @ " << E.getTarget().getAddress()
                   << ">";
          outs() << ", addend = " << E.getAddend() << "\n";
        }
      }
    }
    outs() <<"  External symbols:\n";
    for (auto *Sym : G.external_symbols())
      outs() << "    " << Sym->getName() << "\n";
    outs() << "\n";

    return Error::success();
  }
};

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  ExitOnError ExitOnErr("kaleidoscope: ");

  std::unique_ptr<KaleidoscopeJIT> J = ExitOnErr(KaleidoscopeJIT::Create());

  auto &ProcessSymbolsJD = J->ES->createBareJITDylib("<Process_Symbols>");
  ProcessSymbolsJD.addGenerator(ExitOnErr(
    EPCDynamicLibrarySearchGenerator::GetForTargetProcess(*J->ES)));
  J->MainJD.addToLinkOrder(ProcessSymbolsJD);

  J->ObjLinkingLayer.addPlugin(std::make_unique<MyPlugin>());

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
    auto ParseResult = P.parse(*Line);
    if (!ParseResult)
      continue;

    // If the parser generated a function <func-name> then
    //   (1) rename the function in the AST to <func-name>$impl
    //   (2) Create lazy-reexport map from <func-name> to <func-name>$impl
    //   (3) add the AST and lazy-reexports to the JIT
    std::string FnImplName = ParseResult->FnAST->getName() + "$impl";
    SymbolAliasMap ReExports({
        { J->Mangle(ParseResult->FnAST->getName()),
          { J->Mangle(FnImplName),
            JITSymbolFlags::Exported | JITSymbolFlags::Callable }
        }
      });
    ParseResult->FnAST->setName(std::move(FnImplName));

    ExitOnErr(J->MainJD.define(
        std::make_unique<KaleidoscopeASTMU>(P, *J,
                                            std::move(ParseResult->FnAST))));

    ExitOnErr(J->MainJD.define(
          lazyReexports(LCTM, *ISM, J->MainJD, std::move(ReExports))));

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
