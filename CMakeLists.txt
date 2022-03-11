cmake_minimum_required(VERSION 3.8)

execute_process(COMMAND uname "-s"
    RESULTS_VARIABLE CMAKE_OS_TYPE
)

include_directories("flog/src/include")

project(nvchunk C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_CXX_FLAGS 
    "${CMAKE_CXX_FLAGS} -Wextra -Wall -Werror -Wno-format-nonliteral -Wno-format-security")
SET(CMAKE_CXX_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE} -g")

# add gcov build type
if( ${CMAKE_BUILD_TYPE} STREQUAL Coverage )
    message("* Adding build types...")
    SET(CMAKE_CXX_FLAGS_COVERAGE
        "${CMAKE_CXX_FLAGS_DEBUG} -coverage")
    SET(CMAKE_C_FLAGS_COVERAGE
        "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
endif()

add_subdirectory(flog)

include(CTest)
enable_testing()
add_subdirectory(src)
