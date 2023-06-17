# CI

set(CTEST_BUILD_NAME
  ${CMAKE_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE})
enable_testing()

add_custom_target(TutorialsAsUnitTests)
set_target_properties(TutorialsAsUnitTests PROPERTIES FOLDER "Tutorials as tests")

# Specify COMMAND for passing a custom command to run
function(add_exercise_as_test name)
  cmake_parse_arguments(ARG "" "" "COMMAND" ${ARGN})
  add_dependencies(TutorialsAsUnitTests ${name})
  if (ARG_COMMAND)
    set(CMD ${ARG_COMMAND})
    string(REPLACE ";|;" " | " CMD ${CMD})
  else()
    set(CMD $<TARGET_FILE:${name}>)
  endif()

  add_test(NAME tutorial-${T} COMMAND bash -c ${CMD})
  set_tests_properties(tutorial-${name} PROPERTIES
                       TIMEOUT 2400
                       LABELS
                       DEPENDS)
endfunction(add_exercise_as_test)


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
  add_exercise_as_test(${tgt} COMMAND
    [=[echo -e 'def add(a b) a + b\\\;\nadd(1, 2)\\\;']=] | $<TARGET_FILE:${tgt}>
    )
endfunction()

list(APPEND ENABLED_TUTORIALS p3-ex1 p3-ex4)

if(LLVM_VERSION_MAJOR VERSION_GREATER 16)
  list(APPEND ENABLED_TUTORIALS p1-ex3 p1-ex4 p2-ex1 p2-ex2 p2-ex3 p2-ex4 p2-ex5 p3-ex2 p3-ex3)
endif()

foreach(T IN LISTS ENABLED_TUTORIALS)
  add_subdirectory(${T})
endforeach()

add_custom_target(check-tutorials COMMAND ${CMAKE_CTEST_COMMAND} -V --output-on-failure
  DEPENDS TutorialsAsUnitTests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(check-tutorials PROPERTIES FOLDER "Tutorials as tests")
