cmake_minimum_required(VERSION 3.8)

execute_process(COMMAND uname "-s"
    RESULTS_VARIABLE CMAKE_OS_TYPE
)

include_directories("googletest/googletest/include/")
include_directories("flog/src/include/")

project(nvchunk C CXX)

add_subdirectory(googletest)
add_subdirectory(flog)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# add gcov flag
SET(CMAKE_CXX_FLAGS 
    "${CMAKE_CXX_FLAGS} -Werror -Wno-format-nonliteral -Wno-format-security")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} --coverage")
SET(CMAKE_CXX_FLAGS_DEBUG 
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} --coverage")

include(CTest)
enable_testing()
add_subdirectory(src)
