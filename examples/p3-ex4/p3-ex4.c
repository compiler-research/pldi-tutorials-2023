#include "p3-ex4-lib.h"

int main(int argc, char **argv) {
  Clang_Parse("void* operator new(__SIZE_TYPE__, void* __p) noexcept;"
              "extern \"C\" int printf(const char*,...);"
              "class A {};"
              "class C {};"
              "\n #include <typeinfo> \n"
              "class B : public A, public C {"
              "public:"
              "  template<typename T, typename S, typename U>"
              "  void callme(T, S, U*) {"
              "    printf(\" Instantiated with [%s, %s, %s] \\n \", typeid(T).name(), typeid(S).name(), typeid(U).name());"
              " }"
              "};");
  Decl_t Instantiation = 0;
  const char * InstantiationArgs = "A, int, C*";
  Decl_t TemplatedClass = Clang_LookupName("B", /*Context=*/0);
  Decl_t U = 0;
  Decl_t T = 0;
  if (argc > 1) {
    const char* Code = argv[1];
    Clang_Parse(Code);
    U = Clang_LookupName(argv[2], /*Context=*/0);
    T = Clang_LookupName(argv[3], /*Context=*/0);
    InstantiationArgs = argv[4];
  } else {
    U = Clang_LookupName("A", /*Context=*/0);
    T = Clang_LookupName("C", /*Context=*/0);
  }
  // Instantiate B::callme with the given types
  Instantiation = Clang_InstantiateTemplate(TemplatedClass, "callme", InstantiationArgs);

  // Get the symbol to call
  typedef void (*fn_def)(void*, int, void*);
  fn_def callme_fn_ptr = (fn_def) Clang_GetFunctionAddress(Instantiation);

  // Create objects of type A, B, C
  void* NewA = Clang_CreateObject(U);
  //void* NewB = Clang_CreateObject(B);
  void* NewC = Clang_CreateObject(T);

  callme_fn_ptr(NewA, 42, NewC);

  return 0;
}
