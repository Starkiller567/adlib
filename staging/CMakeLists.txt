project(adlib-staging)

set(EXECUTABLES
  avl_tree
  bitmap
  cbuf
  dbuf
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
