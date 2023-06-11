// You can try this optimization pass by following commands:
// (on mac)
//   make p1-ex3
//   $LLVM_DIR/bin/opt -load-pass-plugin=$TUTORIAL_BUILD_DIR/lib/p1-ex3.dylib -passes="modopt,dce" -S ex.ll
// (on linux)
//   make p1-ex3
//   $LLVM_DIR/bin/opt -load-pass-plugin=$TUTORIAL_BUILD_DIR/lib/p1-ex3.lib -passes="modopt,dce" -S ex.ll
// You should see srem instruction is replaced by select instruction.

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace PatternMatch;

namespace {
  
struct ModOpt : PassInfoMixin<ModOpt> {
  PreservedAnalyses run(Function &Func, FunctionAnalysisManager &) {
    for (auto& BasicBlock : Func) {
      for (auto& Inst : BasicBlock) {
        Value *A = nullptr, *B = nullptr, *Mod = nullptr;
        // Pattern match (A + B) % MOD
        if (match(&Inst, m_SRem(m_Add(m_Value(A), m_Value(B)), m_Value(Mod)))) {
          IRBuilder<> Builder(&Inst);
          auto* Add = Builder.CreateAdd(A, B);
          // Cmp = A + B >= MOD
          auto* Cmp = Builder.CreateICmp(ICmpInst::ICMP_SGE, Add, Mod);
          // Inst <- Cmp ? (A+B-MOD) : (A+B)
          Inst.replaceAllUsesWith(Builder.CreateSelect(Cmp, Builder.CreateSub(Add, Mod), Add));
        }
      }
    }
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};


/* New PM Registration */
llvm::PassPluginLibraryInfo getModOptPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ModOpt", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::FunctionPassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "modopt") {
                    PM.addPass(ModOpt());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getModOptPluginInfo();
}

}