cmake_minimum_required(VERSION 3.8)
project(nvchunk C CXX)

include_directories("googletest/googletest/include/")
include_directories("/opt/rh/devtoolset-9/root/usr/include/c++/9/")

add_subdirectory(googletest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_COMPILER /opt/rh/devtoolset-9/root/bin/cc)
set(CMAKE_CXX_COMPILER /opt/rh/devtoolset-9/root/bin/g++)

include(CTest)
enable_testing()
add_subdirectory(src)
