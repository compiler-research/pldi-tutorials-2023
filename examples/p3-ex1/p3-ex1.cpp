/* See the LICENSE file in the project root for license terms. */

/// This file demonstrates how we could embed Clang and use it as a library in a
/// codebase.

#include "clang/AST/Comment.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Tooling/Tooling.h"

const char* Code = R"(
/// This is the documentation for the ComplexNumber.
// This comment won't appear in the documentation!
template<typename T>
struct ComplexNumber { T real; T imag; };

ComplexNumber<int> c; // variable
)";

int main() {
  using namespace clang;
  auto ASTU = tooling::buildASTFromCodeWithArgs(Code, /*Args=*/{"-std=c++20"});
  ASTContext &C = ASTU->getASTContext();
  TranslationUnitDecl* TU = C.getTranslationUnitDecl();
  // TU->dump();

  for (Decl *D : TU->decls()) {
    if (!isa<ClassTemplateDecl>(D))
      continue;
    comments::FullComment* FC = C.getCommentForDecl(D, &ASTU->getPreprocessor());
    // FC->dump();
    auto* PC = cast<comments::ParagraphComment>(*FC->child_begin());
    auto* TC = cast<comments::TextComment>(*PC->child_begin());
    printf("Comment: '%s'\n", TC->getText().str().data());
  }
  // Can we feed more code and use clang as a service?
}
