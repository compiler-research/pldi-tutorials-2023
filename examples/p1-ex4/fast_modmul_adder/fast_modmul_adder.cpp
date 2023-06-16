#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"

using namespace llvm;
using namespace PatternMatch;

namespace {
struct ModMulAdder : PassInfoMixin<ModMulAdder> {
  ModMulAdder(StringRef ModLLPath) : ModLLPath(ModLLPath) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    SMDiagnostic Err;
    auto ModMul = parseIRFile(ModLLPath, Err, M.getContext());
    Linker::linkModules(M, std::move(ModMul));
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
  StringRef ModLLPath;
};

llvm::PassPluginLibraryInfo getFastModMulAdderPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "fast_modmul_adder", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::ModulePassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "modmuladder") {
                    PM.addPass(ModMulAdder("fast_modmul.bc"));
                    return true;
                  }
                  return false;
                });
          }};
}
}

extern "C" LLVM_ATTRIBUTE_WEAK::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getFastModMulAdderPluginInfo();
}
