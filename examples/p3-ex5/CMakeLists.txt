set(LLVM_OPTIONAL_SOURCES p3-ex5.c p3-ex5-lib.cpp)
set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_llvm_library(p3-ex5-lib SHARED p3-ex5-lib.cpp
  LINK_LIBS
  clangAST
  clangSema
  clangInterpreter
  )
set_source_files_properties(p3-ex5-lib.cpp
  PROPERTIES COMPILE_DEFINITIONS "LLVM_BINARY_DIR=\"${LLVM_BINARY_DIR}\"")
add_llvm_executable(p3-ex5 p3-ex5.c)
target_link_libraries(p3-ex5
  PRIVATE
  p3-ex5-lib
  )
