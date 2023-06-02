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




#include "p3-ex5-lib.h"

#include "clang/Interpreter/Interpreter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/TemplateDeduction.h"

#include "llvm/Support/TargetSelect.h"

#include <memory>
#include <vector>
#include <sstream>

using namespace clang;

llvm::ExitOnError ExitOnErr;

static std::unique_ptr<clang::Interpreter> CreateInterpreter() {
  clang::IncrementalCompilerBuilder CB;
  CB.SetCompilerArgs({"-std=c++17"});

  // Create the incremental compiler instance.
  std::unique_ptr<clang::CompilerInstance> CI;
  CI = ExitOnErr(CB.CreateCpp());

  // Create the interpreter instance.
  std::unique_ptr<Interpreter> Interp
      = ExitOnErr(Interpreter::create(std::move(CI)));

  return ExitOnErr(Interpreter::create(std::move(CI)));
}

// static std::unique_ptr<clang::Interpreter> CreateInterpreter() {
//   std::vector<const char *> ClangArgv = {"-Xclang", "-emit-llvm-only"};
//   auto CI = llvm::cantFail(IncrementalCompilerBuilder::create(ClangArgv));
//   return llvm::cantFail(Interpreter::create(std::move(CI)));
// }

struct LLVMInitRAII {
  LLVMInitRAII() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
  }
  ~LLVMInitRAII() {llvm::llvm_shutdown();}
} LLVMInit;

/// FIXME: Leaks the interpreter object due to D107087.
auto Interp = CreateInterpreter().release();

void Clang_Parse(const char* Code) {
  ExitOnErr(Interp->Parse(Code));
}

static LookupResult LookupName(Sema &SemaRef, const char* Name) {
  ASTContext &C = SemaRef.getASTContext();
  DeclarationName DeclName = &C.Idents.get(Name);
  LookupResult R(SemaRef, DeclName, SourceLocation(), Sema::LookupOrdinaryName);
  SemaRef.LookupName(R, SemaRef.TUScope);
  assert(!R.empty());
  return R;
}

Decl_t Clang_LookupName(const char* Name, Decl_t Context /*=0*/) {
  return LookupName(Interp->getCompilerInstance()->getSema(), Name).getFoundDecl();
}

FnAddr_t Clang_GetFunctionAddress(Decl_t D) {
  clang::FunctionDecl *FD = static_cast<clang::FunctionDecl*>(D);
  auto Addr = Interp->getSymbolAddress(FD);
  if (!Addr)
    return 0;
  //return Addr.toPtr<void*>();
  return Addr->getValue();
  //return *Addr;
}

void * Clang_CreateObject(Decl_t RecordDecl) {
  clang::TypeDecl *TD = static_cast<clang::TypeDecl*>(RecordDecl);
  std::string Name = TD->getQualifiedNameAsString();
  const clang::Type *RDTy = TD->getTypeForDecl();
  clang::ASTContext &C = Interp->getCompilerInstance()->getASTContext();
  size_t size = C.getTypeSize(RDTy);
  void * loc = malloc(size);

  // Tell the interpreter to call the default ctor with this memory. Synthesize:
  // new (loc) ClassName;
  static unsigned counter = 0;
  std::stringstream ss;
  ss << "auto _v" << counter++ << " = " << "new ((void*)" << loc << ")" << Name << "();";

  auto R = Interp->ParseAndExecute(ss.str());
  if (!R)
    return nullptr;

  return loc;
}

/// auto f = &B::callme<A, int, C*>;
Decl_t Clang_InstantiateTemplate(Decl_t Scope, const char* Name, const char* Args) {
  static unsigned counter = 0;
  std::stringstream ss;
  NamedDecl *ND = static_cast<NamedDecl*>(Scope);
  // Args is empty.
  // FIXME: Here we should call Sema::DeduceTemplateArguments (for fn addr) and
  // extend it such that if the substitution is unsuccessful to get out the list
  // of failed candidates, eg TemplateSpecCandidateSet.
  ss << "auto _t" << counter++ << " = &" << ND->getNameAsString() << "::"
     << Name;
  llvm::StringRef ArgList = Args;
  if (!ArgList.empty())
    ss << '<' << Args << '>';
  ss  << ';';
  auto PTU1 = &llvm::cantFail(Interp->Parse(ss.str()));
  llvm::cantFail(Interp->Execute(*PTU1));

  //PTU1->TUPart->dump();

  VarDecl *VD = static_cast<VarDecl*>(*PTU1->TUPart->decls_begin());
  UnaryOperator *UO = llvm::cast<UnaryOperator>(VD->getInit());
  return llvm::cast<DeclRefExpr>(UO->getSubExpr())->getDecl();
}
