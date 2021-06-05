project(adlib-tests)

option(TESTS_ENABLE_SANITIZERS "enable sanitizers for the tests" OFF)
if(${TESTS_ENABLE_SANITIZERS})
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

set(TESTS
  array
  array_benchmark
  avl_tree
  dbuf
  hashmap
  hashset
  hashtable_benchmark
  random
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
