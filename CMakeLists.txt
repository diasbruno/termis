cmake_minimum_required(VERSION 3.25)

project(Termis
  VERSION 0.1.0
  DESCRIPTION "Termis programming language compiler"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(termis_core
  src/codegen.cpp
  src/layout.cpp
  src/reader.cpp
  src/semantic.cpp
  src/type.cpp)

target_include_directories(termis_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(termis_core PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_executable(termisc
  src/main.cpp)

target_link_libraries(termisc PRIVATE termis_core)

target_compile_options(termisc PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

enable_testing()

add_test(NAME termisc_version
  COMMAND termisc --version)

add_test(NAME termisc_help
  COMMAND termisc --help)

add_test(NAME termisc_dump_llvm
  COMMAND termisc --dump-llvm ${CMAKE_CURRENT_SOURCE_DIR}/examples/add.termis)

add_executable(reader_tests
  tests/reader_tests.cpp)

target_link_libraries(reader_tests PRIVATE termis_core)

target_compile_options(reader_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_test(NAME reader_tests
  COMMAND reader_tests)

add_executable(semantic_tests
  tests/semantic_tests.cpp)

target_link_libraries(semantic_tests PRIVATE termis_core)

target_compile_options(semantic_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_test(NAME semantic_tests
  COMMAND semantic_tests)

add_executable(type_tests
  tests/type_tests.cpp)

target_link_libraries(type_tests PRIVATE termis_core)

target_compile_options(type_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_test(NAME type_tests
  COMMAND type_tests)

add_executable(layout_tests
  tests/layout_tests.cpp)

target_link_libraries(layout_tests PRIVATE termis_core)

target_compile_options(layout_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_test(NAME layout_tests
  COMMAND layout_tests)

add_executable(codegen_tests
  tests/codegen_tests.cpp)

target_link_libraries(codegen_tests PRIVATE termis_core)

target_compile_options(codegen_tests PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

add_test(NAME codegen_tests
  COMMAND codegen_tests)
