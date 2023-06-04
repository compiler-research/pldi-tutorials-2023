set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  LineEditor
  Support
)
add_llvm_executable(p3-ex2 p3-ex2.cpp)
target_link_libraries(p3-ex2
  PRIVATE
  clangInterpreter
  )
# Makes the binary symbols visible to the JIT.
export_executable_symbols(p3-ex2)
