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
