set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_llvm_executable(p3-ex3 p3-ex3.cpp)
target_link_libraries(p3-ex3
  PRIVATE
  clangTooling
  clangInterpreter
  )
# Makes the binary symbols visible to the JIT.
export_executable_symbols(p3-ex3)
add_exercise_as_test(p3-ex3)
