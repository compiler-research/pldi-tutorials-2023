#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;
using namespace PatternMatch;
namespace {
  
struct ModOpt : PassInfoMixin<ModOpt> {
  PreservedAnalyses run(Function &Func, FunctionAnalysisManager &) {
    for (auto& BasicBlock : Func) {
      for (auto& Inst : BasicBlock) {
        Value *A = nullptr, *B = nullptr, *C = nullptr, *Mod = nullptr;
        if (match(&Inst, m_SRem(m_Sub(m_Add(m_Value(A), m_Value(C)), m_Value(B)), m_Value(Mod)))) {
          if (C != Mod) continue; 
          // Optimize (A - B + MOD) % MOD
          IRBuilder<> Builder(&Inst);
          auto* Sub = Builder.CreateSub(A, B);
          // Cmp = A - B < 0
          auto* Cmp = Builder.CreateICmp(ICmpInst::ICMP_SLT, A, B);
          Inst.replaceAllUsesWith(Builder.CreateSelect(Cmp, Builder.CreateAdd(Sub, Mod), Sub));
        } else if (match(&Inst, m_SRem(m_Add(m_Value(A), m_Value(B)), m_Value(Mod)))) {
          // Optimize (A + B) % MOD
          IRBuilder<> Builder(&Inst);
          auto* Add = Builder.CreateAdd(A, B);
          auto* Cmp = Builder.CreateICmp(ICmpInst::ICMP_SGE, Add, Mod);
          Inst.replaceAllUsesWith(Builder.CreateSelect(Cmp, Builder.CreateSub(Add, Mod), Add));
        } else if (match(&Inst, m_SRem(m_Mul(m_Value(A), m_Value(B)), m_Value(Mod)))) {
          // TODO
          /*
          %4 = mul nsw i64 %1, %0, !dbg !22
          %5 = sext i32 %2 to i64, !dbg !23
          %6 = sitofp i32 %2 to x86_fp80, !dbg !24
          %7 = fdiv x86_fp80 0xK3FFF8000000000000000, %6, !dbg !25
          %8 = sitofp i64 %0 to x86_fp80, !dbg !26
          %9 = fmul x86_fp80 %7, %8, !dbg !27
          %10 = sitofp i64 %1 to x86_fp80, !dbg !28
          %11 = fmul x86_fp80 %9, %10, !dbg !29
          %12 = fptosi x86_fp80 %11 to i64, !dbg !30
          %13 = mul nsw i64 %12, %5, !dbg !31
          %14 = sub nsw i64 %4, %13, !dbg !32
          call void @llvm.dbg.value(metadata i64 %14, metadata !20, metadata !DIExpression()), !dbg !21
          %15 = lshr i64 %14, 63, !dbg !33
          %16 = trunc i64 %15 to i32, !dbg !33
          %17 = mul nuw nsw i32 %16, %2, !dbg !34
          %18 = icmp slt i64 %14, %5, !dbg !35
          %19 = zext i32 %2 to i64, !dbg !36
          %20 = select i1 %18, i64 0, i64 %19, !dbg !36
          %21 = sub i64 %14, %20, !dbg !37
          %22 = trunc i64 %21 to i32, !dbg !38
          %23 = add i32 %17, %22, !dbg !38
          */
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