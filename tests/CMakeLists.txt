project(adlib-tests)

set(TESTS
  array
)

foreach(TEST IN LISTS TESTS)
  add_executable(${TEST} ${TEST}.c)
  target_link_libraries(${TEST} ad-static)
endforeach()

add_test(array array)
