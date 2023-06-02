set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_llvm_executable(p3ex1 p3-ex1.cpp)
target_link_libraries(p3ex1
  PRIVATE
  clangAST
  clangFrontend
  clangSerialization
  clangTooling
  )
