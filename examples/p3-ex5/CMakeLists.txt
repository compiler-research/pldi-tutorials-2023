set(LLVM_OPTIONAL_SOURCES p3-ex5.cpp p3-ex5-lib.cpp)
set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_library(p3-ex5-lib STATIC p3-ex5-lib.cpp)
target_link_libraries(p3-ex5-lib
  #LINK_LIBS
  clangAST
  clangSema
  clangInterpreter
  )
add_llvm_executable(p3-ex5 p3-ex5.cpp)
target_link_libraries(p3-ex5
  PRIVATE
  p3-ex5-lib
  )
