/* See the LICENSE file in the project root for license terms. */

#include "p3-ex4-lib.h"

const char* Code = "void* operator new(__SIZE_TYPE__, void* __p) noexcept;"
               "extern \"C\" int printf(const char*,...);"
               "class A {};"
               "\n #include <typeinfo> \n"
               "class B {"
               "public:"
               "  template<typename T>"
               "  void callme(T) {"
               "    printf(\" Instantiated with [%s] \\n \", typeid(T).name());"
               " }"
               "};";

int main(int argc, char **argv) {
  Clang_Parse(Code);
  Decl_t Instantiation = 0;
  const char * InstantiationArgs = "A";
  Decl_t TemplatedClass = Clang_LookupName("B", /*Context=*/0);
  Decl_t T = 0;
  if (argc > 1) {
    const char* Code = argv[1];
    Clang_Parse(Code);
    T = Clang_LookupName(argv[2], /*Context=*/0);
    InstantiationArgs = argv[3];
  } else {
    T = Clang_LookupName("A", /*Context=*/0);
  }
  // Instantiate B::callme with the given types
  Instantiation = Clang_InstantiateTemplate(TemplatedClass, "callme", InstantiationArgs);

  // Get the symbol to call
  typedef void (*fn_def)(void*);
  fn_def callme_fn_ptr = (fn_def) Clang_GetFunctionAddress(Instantiation);

  // Create objects of type A
  void* NewA = Clang_CreateObject(T);

  callme_fn_ptr(NewA);

  return 0;
}
