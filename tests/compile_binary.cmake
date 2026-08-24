set(output "${TERMIS_BINARY_DIR}/termis-add")

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
