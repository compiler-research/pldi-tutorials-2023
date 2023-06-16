#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"

using namespace llvm;
using namespace PatternMatch;
namespace {

struct ModOpt : PassInfoMixin<ModOpt> {
  ModOpt() {}
  PreservedAnalyses run(Function &Func, FunctionAnalysisManager &) {
    for (auto &BasicBlock : Func) {
      for (auto &Inst : BasicBlock) {
        Value *A = nullptr, *B = nullptr, *C = nullptr, *Mod = nullptr;
        if (match(&Inst, m_SRem(m_Mul(m_Value(A), m_Value(B)), m_Value(Mod)))) {
          IRBuilder<> Builder(&Inst);
          Function *ModMul = Func.getParent()->getFunction("fast_modmul");
          auto Call = Builder.CreateCall(ModMul, {A, B, Mod});
          Inst.replaceAllUsesWith(Call);
          InlineFunctionInfo IFI;
          InlineFunction(*Call, IFI);
        } else if (match(&Inst, m_SRem(m_Sub(m_Add(m_Value(A), m_Value(C)),
                                             m_Value(B)),
                                       m_Value(Mod)))) {
          if (C != Mod)
            continue;
          // Optimize (A - B + MOD) % MOD
          IRBuilder<> Builder(&Inst);
          auto *Sub = Builder.CreateSub(A, B);
          // Cmp = A - B < 0
          auto *Cmp = Builder.CreateICmp(ICmpInst::ICMP_SLT, A, B);
          Inst.replaceAllUsesWith(
              Builder.CreateSelect(Cmp, Builder.CreateAdd(Sub, Mod), Sub));
        } else if (match(&Inst,
                         m_SRem(m_Add(m_Value(A), m_Value(B)), m_Value(Mod)))) {
          // Optimize (A + B) % MOD
          IRBuilder<> Builder(&Inst);
          auto *Add = Builder.CreateAdd(A, B);
          auto *Cmp = Builder.CreateICmp(ICmpInst::ICMP_SGE, Add, Mod);
          Inst.replaceAllUsesWith(
              Builder.CreateSelect(Cmp, Builder.CreateSub(Add, Mod), Add));
        }
      }
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

llvm::PassPluginLibraryInfo getModOptPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "modopt", LLVM_VERSION_STRING,
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
}

extern "C" LLVM_ATTRIBUTE_WEAK::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getModOptPluginInfo();
}
