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

typedef void* Decl_t;
typedef unsigned long FnAddr_t;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
  /// Process C++ code.
  ///
  void Clang_Parse(const char* Code);

  /// Looks up an entity with the given name, possibly in the given Context.
  ///
  Decl_t Clang_LookupName(const char* Name, Decl_t Context);

  /// Returns the address of a JIT'd function of the corresponding declaration.
  ///
  FnAddr_t Clang_GetFunctionAddress(Decl_t D);

  /// Allocates memory of underlying size of the passed declaration.
  ///
  void * Clang_CreateObject(Decl_t RecordDecl);

  /// Instantiates a given templated declaration.
  Decl_t Clang_InstantiateTemplate(Decl_t D, const char* Name, const char* Args);
#ifdef __cplusplus
}
#endif // __cplusplus
