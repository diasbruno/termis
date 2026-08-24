cmake_minimum_required(VERSION 3.25)

project(Termis
  VERSION 0.1.0
  DESCRIPTION "Termis programming language compiler"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(termisc
  src/main.cpp)

target_compile_options(termisc PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wpedantic>)

enable_testing()

add_test(NAME termisc_version
  COMMAND termisc --version)

add_test(NAME termisc_help
  COMMAND termisc --help)
