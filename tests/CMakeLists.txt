project(adlib-tests)

set(TESTS
  array
  array_benchmark
  avl_tree
  dbuf
  dstring
  dstring_number_conversion
  hash
  hash_benchmark
  hashmap
  hashset
  hashtable_benchmark
  json
  random
  random_benchmark
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

target_link_libraries(hash m)
target_link_libraries(random m)

add_executable(tests testing.c utils.c)
if(${TESTS_USE_SINGLE_HEADERS})
  target_include_directories(tests PRIVATE ${SINGLE_HEADERS_DIR})
  add_dependencies(tests single_header_library)
else()
  target_link_libraries(tests ad-static)
endif()
target_link_libraries(tests m)
