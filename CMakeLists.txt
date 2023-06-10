cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
  )

# If we are not building as a part of LLVM, build InterOp as a standalone
# project, using LLVM as an external library:
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
  project(InterOp)

  # LLVM/Clang default paths
  if (DEFINED LLVM_DIR)
    if (NOT DEFINED Clang_DIR)
      set(Clang_DIR ${LLVM_DIR})
    endif()
  endif()
  if (DEFINED Clang_DIR)
    if (NOT DEFINED LLVM_DIR)
      set(LLVM_DIR ${Clang_DIR})
    endif()
  endif()
  
  ## Define supported version of clang and llvm

  set(CLANG_MIN_SUPPORTED 17.0)
  set(CLANG_MAX_SUPPORTED "17.0.x")
  set(CLANG_VERSION_UPPER_BOUND 17.1.0)
  set(LLVM_MIN_SUPPORTED 17.0)
  set(LLVM_MAX_SUPPORTED "17.0.x")
  set(LLVM_VERSION_UPPER_BOUND 17.1.0)

  ## Set Cmake packages search order

  set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
  set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

  ## Search packages HINTS and PATHS

  if (DEFINED LLVM_DIR)
    set(llvm_search_hints PATHS ${LLVM_DIR} HINTS "${LLVM_DIR}/lib/cmake/llvm" "${LLVM_DIR}/cmake" "${LLVM_CONFIG_EXTRA_PATH_HINTS}")
  endif()
  if (DEFINED Clang_DIR)
    set(clang_search_hints PATHS ${Clang_DIR} HINTS "${Clang_DIR}/lib/cmake/clang" "${Clang_DIR}/cmake" "${CLANG_CONFIG_EXTRA_PATH_HINTS}")
  endif()

  ## Find supported LLVM

  find_package(LLVM REQUIRED CONFIG ${llvm_search_hints} NO_DEFAULT_PATH)
  find_package(Clang REQUIRED CONFIG ${clang_search_hints} NO_DEFAULT_PATH)

  if (LLVM_FOUND)
    if (LLVM_PACKAGE_VERSION VERSION_LESS LLVM_MIN_SUPPORTED OR LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL LLVM_VERSION_UPPER_BOUND)
      unset(LLVM_FOUND)
      unset(LLVM_VERSION_MAJOR)
      unset(LLVM_VERSION_MINOR)
      unset(LLVM_VERSION_PATCH)
      unset(LLVM_PACKAGE_VERSION)
    else()
      if (NOT DEFINED LLVM_VERSION AND NOT DEFINED LLVM_DIR)
        set(LLVM_VERSION ${LLVM_PACKAGE_VERSION})
      endif()
    endif()
  endif()

  if (NOT LLVM_FOUND AND DEFINED LLVM_VERSION)
    if (LLVM_VERSION VERSION_GREATER_EQUAL LLVM_VERSION_UPPER_BOUND)
      set(LLVM_VERSION ${LLVM_VERSION_UPPER_BOUND})
    endif()
    if (LLVM_VERSION VERSION_LESS LLVM_MIN_SUPPORTED)
      set(LLVM_VERSION ${LLVM_MIN_SUPPORTED})
    endif()

    find_package(LLVM ${LLVM_VERSION} REQUIRED CONFIG ${llvm_search_hints} NO_DEFAULT_PATHS)
  endif()

  if (NOT LLVM_FOUND AND DEFINED LLVM_DIR)
    find_package(LLVM REQUIRED CONFIG ${llvm_search_hints} NO_DEFAULT_PATH)
  endif()

  if (NOT LLVM_FOUND)
    find_package(LLVM REQUIRED CONFIG)
  endif()

  if (NOT LLVM_FOUND)
    message(FATAL_ERROR "Please set LLVM_DIR pointing to the LLVM build or installation folder")
  endif()

  if (LLVM_PACKAGE_VERSION VERSION_LESS LLVM_MIN_SUPPORTED OR LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL LLVM_VERSION_UPPER_BOUND)
    message(FATAL_ERROR "Found unsupported version: LLVM ${LLVM_PACKAGE_VERSION};\nPlease set LLVM_DIR pointing to the llvm version ${LLVM_MIN_SUPPORTED} to ${LLVM_MAX_SUPPORTED} build or installation folder")
  endif()

  message(STATUS "Found supported version: LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  ## Find supported Clang

  if (DEFINED CLANG_VERSION)
    if (CLANG_VERSION VERSION_GREATER_EQUAL CLANG_VERSION_UPPER_BOUND)
      set(CLANG_VERSION ${CLANG_VERSION_UPPER_BOUND})
    endif()
    if (CLANG_VERSION VERSION_LESS CLANG_MIN_SUPPORTED)
      set(CLANG_VERSION ${CLANG_MIN_SUPPORTED})
    endif()

    find_package(Clang ${CLANG_VERSION} REQUIRED CONFIG ${clang_extra_hints} NO_DEFAULT_PATH)
  endif()

  if (NOT Clang_FOUND AND DEFINED Clang_DIR)
    find_package(Clang REQUIRED CONFIG ${clang_extra_hints} NO_DEFAULT_PATH)
  endif()

  if (NOT Clang_FOUND)
    find_package(Clang REQUIRED CONFIG)
  endif()

  if (NOT Clang_FOUND)
    message(FATAL_ERROR "Please set Clang_DIR pointing to the clang build or installation folder")
  endif()

  set(CLANG_VERSION_MAJOR ${LLVM_VERSION_MAJOR})
  set(CLANG_VERSION_MINOR ${LLVM_VERSION_MINOR})
  set(CLANG_VERSION_PATCH ${LLVM_VERSION_PATCH})
  set(CLANG_PACKAGE_VERSION ${LLVM_PACKAGE_VERSION})

  if (CLANG_PACKAGE_VERSION VERSION_LESS CLANG_MIN_SUPPORTED OR CLANG_PACKAGE_VERSION VERSION_GREATER_EQUAL CLANG_VERSION_UPPER_BOUND)
    message(FATAL_ERROR "Found unsupported version: Clang ${CLANG_PACKAGE_VERSION};\nPlease set Clang_DIR pointing to the clang version ${CLANG_MIN_SUPPORTED} to ${CLANG_MAX_SUPPORTED} build or installation folder")
  endif()

  message(STATUS "Found supported version: Clang ${CLANG_PACKAGE_VERSION}")
  message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

  ## Clang 13 require c++14 or later, Clang 16 require c++17 or later.
  if (CLANG_VERSION_MAJOR GREATER_EQUAL 16)
    if (NOT CMAKE_CXX_STANDARD)
      set (CMAKE_CXX_STANDARD 17)
    endif()
    if (CMAKE_CXX_STANDARD LESS 17)
      message(fatal "LLVM/InterOp requires c++17 or later")
    endif()
  elseif (CLANG_VERSION_MAJOR GREATER_EQUAL 13)
    if (NOT CMAKE_CXX_STANDARD)
      set (CMAKE_CXX_STANDARD 14)
    endif()
    if (CMAKE_CXX_STANDARD LESS 14)
      message(fatal "LLVM/InterOp requires c++14 or later")
    endif()
  endif()

  ## Init

  # In case this was a path to a build folder of llvm still try to find AddLLVM
  list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}")

  # Fix bug in some AddLLVM.cmake implementation (-rpath "" problem)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib${LLVM_LIBDIR_SUFFIX})

  include(AddLLVM)
  include(HandleLLVMOptions)

  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
  include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  add_definitions(${LLVM_DEFINITIONS_LIST})

  message(STATUS "CLANG_INCLUDE_DIRS: ${CLANG_INCLUDE_DIRS}")
  message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")
  message(STATUS "LLVM_DEFINITIONS_LIST: ${LLVM_DEFINITIONS_LIST}")

  # If the llvm sources are present add them with higher priority.
  if (LLVM_BUILD_MAIN_SRC_DIR)
    # LLVM_INCLUDE_DIRS contains the include paths to both LLVM's source and
    # build directories. Since we cannot just include ClangConfig.cmake (see
    # fixme above) we have to do a little more work to get the right include
    # paths for clang.
    #
    # FIXME: We only support in-tree builds of clang, that is clang being built
    # in llvm_src/tools/clang.
    include_directories(SYSTEM ${LLVM_BUILD_MAIN_SRC_DIR}/tools/clang/include/)

    if (NOT LLVM_BUILD_BINARY_DIR)
      message(FATAL "LLVM_BUILD_* values should be available for the build tree")
    endif()

    include_directories(SYSTEM ${LLVM_BUILD_BINARY_DIR}/tools/clang/include/)
  endif()

  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)

  set( INTEROP_BUILT_STANDALONE 1 )
endif()

# Add appropriate flags for GCC
if (LLVM_COMPILER_IS_GCC_COMPATIBLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings")
endif ()

if (APPLE)
  set(CMAKE_MODULE_LINKER_FLAGS "-Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
endif ()

# FIXME: Use merge this with the content from the LLVMConfig and ClangConfig.
if (NOT INTEROP_BUILT_STANDALONE)
include_directories(BEFORE SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/../clang/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../clang/include
  )
endif()

include_directories(BEFORE SYSTEM
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.def"
  PATTERN "*.h"
  PATTERN ".svn" EXCLUDE
  )

add_definitions( -D_GNU_SOURCE )

# Add deps if we build together with clang.
if (TARGET intrinsics_gen)
  list(APPEND LLVM_COMMON_DEPENDS intrinsics_gen)
endif()
if (TARGET clang-headers)
  list(APPEND LLVM_COMMON_DEPENDS clang-headers)
endif()

add_subdirectory(examples)
