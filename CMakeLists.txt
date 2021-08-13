cmake_minimum_required(VERSION 3.14)
project(adlib)

include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "-Og -g")
# set(CMAKE_C_FLAGS_RELEASE "-O3 -flto -march=native -mtune=native")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -mtune=native")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")

# set(THREADS_PREFER_PTHREAD_FLAG ON)
# find_package(Threads REQUIRED)
# target_link_libraries(my_app PRIVATE Threads::Threads)

check_c_source_compiles("int main() { typeof(int) x = 0; return x; }" HAVE_TYPEOF)
check_c_source_compiles("void __attribute__((nonnull (1))) f(void *a) {} int main() { return 0; }" HAVE_ATTR_NONNULL)
check_c_source_compiles("int __attribute__((warn_unused_result)) f(void *a) {} int main() { return 0; }" HAVE_ATTR_WARN_UNUSED_RESULT)
check_c_source_compiles("void __attribute__((always_inline)) f(void *a) {} int main() { return 0; }" HAVE_ATTR_ALWAYS_INLINE)
check_c_source_compiles("void __attribute__((format (printf, 1, 2))) f(const char *fmt, ...) {} int main() { return 0; }" HAVE_ATTR_FORMAT_PRINTF)
check_c_source_compiles("struct { int i; } __attribute__((packed)); int main() { return 0; }" HAVE_ATTR_PACKED)
check_c_source_compiles("int main() { int x __attribute__((unused)) = 0; return x; }" HAVE_ATTR_UNUSED)
check_c_source_compiles("int main() { if (__builtin_expect(0, 0)); return 0; }" HAVE_BUILTIN_EXPECT)
check_symbol_exists(malloc_usable_size "malloc.h" HAVE_MALLOC_USABLE_SIZE)
if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(memmem "string.h" HAVE_MEMMEM)
  check_symbol_exists(memrchr "string.h" HAVE_MEMRCHR)
  list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
endif()

option(ARRAY_SAFETY_CHECKS "Enable safety assertions in the array implementation" ON)
set(ARRAY_INITIAL_SIZE 8 CACHE STRING "Initial size of an array")
# ARRAY_GROWTH_FACTOR=1.6 (~ golden ratio)
set(ARRAY_GROWTH_FACTOR_NUMERATOR 8 CACHE STRING "Numerator of the array growth factor")
set(ARRAY_GROWTH_FACTOR_DENOMINATOR 5 CACHE STRING "Denominator of the array growth factor")

set(DBUF_INITIAL_SIZE 16 CACHE STRING "Initial size of a dynamic byte buffer (dbuf)")

option(DSTRING_SAFETY_CHECKS "Enable safety assertions in the dstring implementation" ON)
set(DSTRING_INITIAL_SIZE 8 CACHE STRING "Initial size of an dstring")
# DSTRING_GROWTH_FACTOR=1.6 (~ golden ratio)
set(DSTRING_GROWTH_FACTOR_NUMERATOR 8 CACHE STRING "Numerator of the dstring growth factor")
set(DSTRING_GROWTH_FACTOR_DENOMINATOR 5 CACHE STRING "Denominator of the dstring growth factor")

set(HASHTABLE_IMPLEMENTATION "QUADRATIC" CACHE STRING "Hashtable implementation (QUADRATIC/HOPSCOTCH/ROBINHOOD)")
if(HASHTABLE_IMPLEMENTATION STREQUAL "QUADRATIC")
  set(HASHTABLE_QUADRATIC ON CACHE BOOL "")
elseif(HASHTABLE_IMPLEMENTATION STREQUAL "HOPSCOTCH")
  set(HASHTABLE_HOPSCOTCH ON CACHE BOOL "")
elseif(HASHTABLE_IMPLEMENTATION STREQUAL "ROBINHOOD")
  set(HASHTABLE_ROBINHOOD ON CACHE BOOL "")
else()
  message(FATAL_ERROR "Invalid hashtable implementation.")
endif()

configure_file(include/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h)

add_compile_definitions(__AD_LINKAGE=)

set(SOURCES
  array.c
  avl_tree.c
  config.c
  dbuf.c
  dstring.c
  hashtable.c
  macros.c
  random.c
  rb_tree.c
)

foreach(SOURCE IN LISTS SOURCES)
  get_filename_component(BASENAME ${SOURCE} NAME_WLE)
  string(TOUPPER ${BASENAME} OPTION_NAME)
  string(PREPEND OPTION_NAME DISABLE_)
  option(${OPTION_NAME} "disable ${SOURCE}" OFF)
  if(${OPTION_NAME})
    list(REMOVE_ITEM SOURCES ${SOURCE})
  endif()
endforeach()

list(LENGTH SOURCES NUM_SOURCES)
if(${NUM_SOURCES} EQUAL 0)
  message(FATAL_ERROR "You have disabled all sources.")
endif()

list(TRANSFORM SOURCES PREPEND src/)

set(SOURCE_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_STATIC_LIBRARY "build static library" ON)
if(${BUILD_STATIC_LIBRARY})
  add_library(ad-static STATIC ${SOURCES})
  target_include_directories(ad-static PUBLIC
    $<BUILD_INTERFACE:${SOURCE_INCLUDE_DIRECTORY}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
endif()
option(BUILD_SHARED_LIBRARY "build shared library" OFF)
if(${BUILD_SHARED_LIBRARY})
  add_library(ad-shared SHARED ${SOURCES})
  target_include_directories(ad-shared PUBLIC
    $<BUILD_INTERFACE:${SOURCE_INCLUDE_DIRECTORY}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
endif()

option(BUILD_SINGLE_HEADER_LIBRARY "build single header library" ON)
if(${BUILD_SINGLE_HEADER_LIBRARY})
  set(SINGLE_HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_single_header.py)
  set(SINGLE_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/single_headers)
  file(MAKE_DIRECTORY ${SINGLE_HEADERS_DIR})
  # set(DEPDIR ${CMAKE_CURRENT_BINARY_DIR}/depfiles/)
  # file(MAKE_DIRECTORY ${DEPDIR})
  set(SINGLE_HEADER_FILES)
  foreach(SOURCE IN LISTS SOURCES)
    get_filename_component(BASENAME ${SOURCE} NAME_WLE)
    set(DEST ${SINGLE_HEADERS_DIR}/${BASENAME}.h)
    list(APPEND SINGLE_HEADER_FILES ${DEST})
    # set(DEPFILE ${DEPDIR}/${BASENAME}.d)
    # add_custom_command(
    #   OUTPUT ${DEPFILE}
    #   COMMAND ${CMAKE_C_COMPILER} -I${SOURCE_INCLUDE_DIRECTORY} -MM -MT ${DEST} -MF ${DEPFILE} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}
    #   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}
    #   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    # )
    add_custom_command(
      OUTPUT ${DEST}
      COMMAND ${SINGLE_HEADER_GENERATOR} ${SOURCE} ${DEST}
      COMMENT "Generating single header for ${SOURCE}"
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE} ${SINGLE_HEADER_GENERATOR} # ${DEPFILE}
      IMPLICIT_DEPENDS C ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}
      # DEPFILE ${DEPFILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
  endforeach()
  add_custom_target(single_header_library ALL
    SOURCES ${SINGLE_HEADER_GENERATOR}
    DEPENDS ${SINGLE_HEADER_FILES}
  )
  set_property(TARGET single_header_library PROPERTY INCLUDE_DIRECTORIES ${SOURCE_INCLUDE_DIRECTORY})
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

if(${BUILD_SINGLE_HEADER_LIBRARY})
  install(DIRECTORY ${SINGLE_HEADERS_DIR}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h")
endif()
if(${BUILD_STATIC_LIBRARY} OR ${BUILD_SHARED_LIBRARY})
  install(DIRECTORY ${SOURCE_INCLUDE_DIRECTORY}/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h")
endif()
if(${BUILD_STATIC_LIBRARY})
  install(TARGETS ad-static ARCHIVE)
endif()
if(${BUILD_SHARED_LIBRARY})
  install(TARGETS ad-shared LIBRARY)
endif()

if(${BUILD_STATIC_LIBRARY} OR ${BUILD_SINGLE_HEADER_LIBRARY})
  if(${BUILD_SINGLE_HEADER_LIBRARY})
    if(${BUILD_STATIC_LIBRARY})
      option(TESTS_USE_SINGLE_HEADERS "Use the single header library for the tests" OFF)
    else()
      set(TESTS_USE_SINGLE_HEADERS ON)
    endif()
  endif()

  enable_testing()
  add_subdirectory(tests)
endif()

if(${BUILD_STATIC_LIBRARY})
  add_subdirectory(staging)
endif()
