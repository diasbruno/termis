set(output "${TERMIS_BINARY_DIR}/termis-add")
set(stdlib_output "${TERMIS_BINARY_DIR}/termis-stdlib")
set(ffi_output "${TERMIS_BINARY_DIR}/termis-ffi")

execute_process(
  COMMAND "${TERMISC}" -I "${TERMIS_SOURCE_DIR}/std" --dump-llvm "${TERMIS_SOURCE_DIR}/examples/add.termis"
  RESULT_VARIABLE memory_dump_result
  OUTPUT_VARIABLE memory_dump_output
  ERROR_VARIABLE memory_dump_error)

if(NOT memory_dump_result EQUAL 0)
  message(FATAL_ERROR "termisc failed while loading std.memory:\n${memory_dump_output}\n${memory_dump_error}")
endif()

if(NOT memory_dump_output MATCHES "declare ptr @malloc")
  message(FATAL_ERROR "std.memory did not declare malloc after dependency loading")
endif()

execute_process(
  COMMAND "${TERMISC}" -o "${output}" "${TERMIS_SOURCE_DIR}/examples/add.termis"
  RESULT_VARIABLE compile_result
  OUTPUT_VARIABLE compile_output
  ERROR_VARIABLE compile_error)

if(NOT compile_result EQUAL 0)
  message(FATAL_ERROR "termisc failed:\n${compile_output}\n${compile_error}")
endif()

execute_process(
  COMMAND "${output}"
  RESULT_VARIABLE run_result)

if(NOT run_result EQUAL 42)
  message(FATAL_ERROR "compiled binary returned ${run_result}, expected 42")
endif()

execute_process(
  COMMAND "${TERMISC}" -I "${TERMIS_SOURCE_DIR}/std" -o "${stdlib_output}" "${TERMIS_SOURCE_DIR}/examples/stdlib.termis"
  RESULT_VARIABLE stdlib_compile_result
  OUTPUT_VARIABLE stdlib_compile_output
  ERROR_VARIABLE stdlib_compile_error)

if(NOT stdlib_compile_result EQUAL 0)
  message(FATAL_ERROR "termisc failed for stdlib example:\n${stdlib_compile_output}\n${stdlib_compile_error}")
endif()

execute_process(
  COMMAND "${stdlib_output}"
  RESULT_VARIABLE stdlib_run_result)

if(NOT stdlib_run_result EQUAL 42)
  message(FATAL_ERROR "stdlib example returned ${stdlib_run_result}, expected 42")
endif()

execute_process(
  COMMAND "${TERMISC}" -o "${ffi_output}" "${TERMIS_SOURCE_DIR}/examples/ffi.termis"
  RESULT_VARIABLE ffi_compile_result
  OUTPUT_VARIABLE ffi_compile_output
  ERROR_VARIABLE ffi_compile_error)

if(NOT ffi_compile_result EQUAL 0)
  message(FATAL_ERROR "termisc failed for FFI example:\n${ffi_compile_output}\n${ffi_compile_error}")
endif()

execute_process(
  COMMAND "${ffi_output}"
  RESULT_VARIABLE ffi_run_result)

if(NOT ffi_run_result EQUAL 42)
  message(FATAL_ERROR "FFI example returned ${ffi_run_result}, expected 42")
endif()
