cmake_minimum_required(VERSION 3.8)

execute_process(COMMAND uname "-s"
    RESULTS_VARIABLE CMAKE_OS_TYPE
)

if( ${CMAKE_OS_TYPE} STREQUAL "Linux" )
    set(CMAKE_C_COMPILER /opt/rh/devtoolset-9/root/bin/cc)
    set(CMAKE_CXX_COMPILER /opt/rh/devtoolset-9/root/bin/g++)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

project(nvchunk C CXX)

include_directories("googletest/googletest/include/")
include_directories("/opt/rh/devtoolset-9/root/usr/include/c++/9/")


add_subdirectory(googletest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# add gcov flag
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-format-nonliteral -Wno-format-security")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} --coverage")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} --coverage")

include(CTest)
enable_testing()
add_subdirectory(src)
