set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
)
add_llvm_executable(p3ex-bonus p3-ex-bonus.cpp)
target_link_libraries(p3ex-bonus
  PRIVATE
  clangInterpreter
  )
set_source_files_properties(p3ex-bonus.cpp
  PROPERTIES COMPILE_DEFINITIONS "LLVM_BINARY_DIR=\"${LLVM_BINARY_DIR}\"")
# Makes the binary symbols visible to the JIT.
export_executable_symbols(p3ex-bonus)
