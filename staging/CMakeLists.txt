project(adlib-staging)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function -Wno-unused-variable")

set(EXECUTABLES
  avl_tree
  bitmap
  cbuf
  endian
  error
  hashmap_test
  hashset_test
  hashtable_benchmark
  list
  malloc
  mbuf
  mem_arena
  mprintf
  random
  rb_tree
  sort
)

foreach(EXECUTABLE IN LISTS EXECUTABLES)
  add_executable(${EXECUTABLE} ${EXECUTABLE}.c)
  target_link_libraries(${EXECUTABLE} ad-static)
endforeach()

target_link_libraries(random m)
