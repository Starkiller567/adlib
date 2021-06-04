project(adlib-tests)

set(TESTS
  array
  array_perf
  avl_tree
  dbuf
  rb_tree
)

foreach(TEST IN LISTS TESTS)
  add_executable(${TEST} ${TEST}.c)
  if(${TESTS_USE_SINGLE_HEADERS})
    target_include_directories(${TEST} PRIVATE ${SINGLE_HEADERS_DIR})
    add_dependencies(${TEST} single_header_library)
  else()
    target_link_libraries(${TEST} ad-static)
  endif()
  add_test(${TEST} ${TEST})
endforeach()
