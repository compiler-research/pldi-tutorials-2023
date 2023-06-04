llvm_map_components_to_libnames(KALEIDOSCOPE_LLVM_LIBS
  support lineeditor orcjit native core)

function(add_kaleidoscope_exercise ex_name)
  set(tgt "${ex_name}")
  add_llvm_executable("${tgt}" "${ex_name}.cpp" ${CMAKE_SOURCE_DIR}/examples/Kaleidoscope.cpp)
  set_property(TARGET "${tgt}" PROPERTY CXX_STANDARD 17)
  target_include_directories("${tgt}" PRIVATE ${CMAKE_SOURCE_DIR}/examples)
  target_include_directories("${tgt}" PRIVATE ${LLVM_INCLUDE_DIRS})
  target_compile_definitions("${tgt}" PRIVATE ${LLVM_DEFINITIONS})
  target_link_libraries("${tgt}" PRIVATE ${KALEIDOSCOPE_LLVM_LIBS})
endfunction()

list(APPEND ENABLED_TUTORIALS p3-ex1 p3-ex4)

if(LLVM_VERSION_MAJOR VERSION_GREATER 16)
  list(APPEND ENABLED_TUTORIALS p1-ex1 p3-ex3 p3-ex2 p1-ex2 p1-ex3 p1-ex4)
endif()

foreach(T IN LISTS ENABLED_TUTORIALS)
  add_subdirectory(${T})
endforeach()

# CI

set(CTEST_BUILD_NAME
  ${CMAKE_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE})
enable_testing()

add_custom_target(TutorialsAsUnitTests)
set_target_properties(TutorialsAsUnitTests PROPERTIES FOLDER "Tutorials as tests")

# FIXME: We should provide some demo input to these.
list(APPEND INTERACTIVE_TUTORIALS p1-ex1 p1-ex2 p1-ex3 p1-ex4 p1-ex5 p3-ex2)

set (TIMEOUT_VALUE 2400)
# Add all tutorials as tests for ctest.
foreach(T IN LISTS ENABLED_TUTORIALS)
  if (T IN_LIST INTERACTIVE_TUTORIALS)
    continue()
  endif()

  add_dependencies(TutorialsAsUnitTests ${T})
  #set_property(TARGET ${T} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME tutorial-${T} COMMAND ${T})
  set_tests_properties(tutorial-${T} PROPERTIES
                       TIMEOUT "${TIMEOUT_VALUE}"
                       LABELS
                       DEPENDS)
endforeach()

add_custom_target(check-tutorials COMMAND ${CMAKE_CTEST_COMMAND} -V --output-on-failure
  DEPENDS TutorialsAsUnitTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(check-tutorials PROPERTIES FOLDER "Tutorials as tests")
