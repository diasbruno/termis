cmake_minimum_required(VERSION 3.25)

project(Termis
  VERSION 0.1.0
  DESCRIPTION "Termis programming language compiler"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(termis_core
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
