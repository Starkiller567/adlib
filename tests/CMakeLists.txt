project(adlib-tests)

set(STANDALONE_TESTS
  array_benchmark
  hash_benchmark
  hashtable_benchmark
  random_benchmark
)

foreach(TEST IN LISTS STANDALONE_TESTS)
  add_executable(${TEST} ${TEST}.c)
  if(${TESTS_USE_SINGLE_HEADERS})
    target_include_directories(${TEST} PRIVATE ${SINGLE_HEADERS_DIR})
    add_dependencies(${TEST} single_header_library)
  else()
    target_link_libraries(${TEST} ad-static)
  endif()
  add_test(${TEST} ${TEST})
endforeach()

set(TEST_SOURCES
  array.c
  avl_tree.c
  charconv.c
  dbuf.c
  dstring.c
  hash.c
  hashmap.c
  hashset.c
  json.c
  random.c
  rb_tree.c
  utils.c
)

add_executable(tests testing.c ${TEST_SOURCES})
if(${TESTS_USE_SINGLE_HEADERS})
  target_include_directories(tests PRIVATE ${SINGLE_HEADERS_DIR})
  add_dependencies(tests single_header_library)
else()
  target_link_libraries(tests ad-static)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_link_libraries(tests Threads::Threads m)
