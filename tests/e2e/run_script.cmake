# Runs PROGRAM --plain --echo with SCRIPT on standard input. Passes when the program exits 0
# and every line of SCRIPT's .expect file (same name, .expect) appears in the output.
execute_process(COMMAND ${PROGRAM} --plain --echo
                INPUT_FILE ${SCRIPT}
                OUTPUT_VARIABLE output
                ERROR_VARIABLE errors
                RESULT_VARIABLE result
                TIMEOUT 30)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "${PROGRAM} exited with ${result}\n${errors}\n${output}")
endif()
string(REGEX REPLACE "\\.txt$" ".expect" expect_file ${SCRIPT})
if(EXISTS ${expect_file})
  file(STRINGS ${expect_file} expected)
  foreach(line ${expected})
    string(FIND "${output}" "${line}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Missing from output: ${line}\n--- output ---\n${output}")
    endif()
  endforeach()
endif()
