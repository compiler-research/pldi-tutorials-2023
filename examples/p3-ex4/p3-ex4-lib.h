/* See the LICENSE file in the project root for license terms. */

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
