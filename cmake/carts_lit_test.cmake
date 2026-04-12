# IREE-style macro: registers a lit test directory with CTest.
#
# Usage:
#   carts_lit_test(NAME carts-sde-tests
#                  TEST_DIR ${CMAKE_SOURCE_DIR}/lib/arts/dialect/sde/test
#                  LABELS pass
#                  DEPENDS carts-compile)
#
function(carts_lit_test)
  cmake_parse_arguments(ARG "" "NAME;TEST_DIR" "LABELS;DEPENDS" ${ARGN})

  if(NOT ARG_NAME)
    message(FATAL_ERROR "carts_lit_test: NAME is required")
  endif()
  if(NOT ARG_TEST_DIR)
    message(FATAL_ERROR "carts_lit_test: TEST_DIR is required")
  endif()
  if(NOT LLVM_LIT_EXECUTABLE)
    message(WARNING "carts_lit_test: llvm-lit not found, skipping ${ARG_NAME}")
    return()
  endif()

  add_test(
    NAME ${ARG_NAME}
    COMMAND ${LLVM_LIT_EXECUTABLE} ${ARG_TEST_DIR}
            --config-prefix lit.site
            -sv
  )

  set(_props "")
  if(ARG_LABELS)
    list(APPEND _props LABELS "${ARG_LABELS}")
  endif()
  if(ARG_DEPENDS)
    list(APPEND _props DEPENDS "${ARG_DEPENDS}")
  endif()
  if(_props)
    set_tests_properties(${ARG_NAME} PROPERTIES ${_props})
  endif()
endfunction()
