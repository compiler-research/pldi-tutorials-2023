set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_llvm_executable(p3ex3 p3-ex3.cpp)
target_link_libraries(p3ex3
  PRIVATE
  clangTooling
  clangInterpreter
  )
