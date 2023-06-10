/* See the LICENSE file in the project root for license terms. */

#ifndef KALEIDOSCOPE_H
#define KALEIDOSCOPE_H

#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/Support/Error.h"
#include <memory>

namespace llvm {
  class AllocaInst;
  class Function;
  class Module;
  class Value;
} // end namespace llvm

struct CodeGenContext;
struct KaleidoscopeJIT;
struct KaleidoscopeParser;
class PrototypeAST;
class ExprAST;

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body);
  ~FunctionAST();

  const std::string &getName() const;
  void setName(std::string NewName);
  llvm::Function *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx);
};

struct KaleidoscopeParser {

  struct ParseResult {
    std::unique_ptr<FunctionAST> FnAST; // Null for prototypes.
    std::string TopLevelExpr; // Name of expression function, if any.
  };

  KaleidoscopeParser();
  ~KaleidoscopeParser();

  std::optional<ParseResult> parse(llvm::StringRef Code);

  std::optional<llvm::orc::ThreadSafeModule>
  codegen(std::unique_ptr<FunctionAST> FnAST, const llvm::DataLayout &DL);

  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
};

struct KaleidoscopeJIT {
  std::unique_ptr<llvm::orc::ExecutionSession> ES;

  llvm::DataLayout DL;
  llvm::orc::MangleAndInterner Mangle;

  llvm::orc::ObjectLinkingLayer ObjLinkingLayer;
  llvm::orc::IRCompileLayer CompileLayer;

  llvm::orc::JITDylib &MainJD;

  static llvm::Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
    auto EPC = llvm::orc::SelfExecutorProcessControl::Create();
    if (!EPC)
      return EPC.takeError();

    auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(*EPC));

    llvm::orc::JITTargetMachineBuilder JTMB(
        ES->getExecutorProcessControl().getTargetTriple());
    JTMB.setCodeModel(llvm::CodeModel::Small);

    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();

    return std::unique_ptr<KaleidoscopeJIT>(
        new KaleidoscopeJIT(std::move(ES), std::move(JTMB), std::move(*DL)));
  }

  ~KaleidoscopeJIT() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
  }

private:
  KaleidoscopeJIT(std::unique_ptr<llvm::orc::ExecutionSession> ES,
                  llvm::orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL)
      : ES(std::move(ES)), DL(std::move(DL)), Mangle(*this->ES, this->DL),
        ObjLinkingLayer(*this->ES),
        CompileLayer(*this->ES, ObjLinkingLayer,
                     std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(JTMB))),
        MainJD(this->ES->createBareJITDylib("<main>")) {}

};


#endif // KALEIDOSCOPE_H
