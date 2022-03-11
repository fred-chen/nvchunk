
add_executable(nvchunk_test nvchunk_test.cpp)   # 编译主文件

IF(NOT HAVE_LIBPMEM_H)
MESSAGE( STATUS "NO LIBPMEM" )
ELSE()
target_link_libraries(nvchunk_test pmem)
ENDIF()

add_custom_command(
    TARGET nvchunk_test
    POST_BUILD
    COMMAND objcopy --only-keep-debug nvchunk_test nvchunk_test.debug
    COMMAND strip --strip-debug nvchunk_test
    COMMAND objcopy --add-gnu-debuglink=nvchunk_test.debug nvchunk_test
)

add_test(NAME nvchunk_test COMMAND nvchunk_test)

add_custom_target(
    gcov
    DEPENDS gcov.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
)

add_custom_command(
    OUTPUT gcov.txt
    COMMAND ctest -V
    COMMAND eval "lcov --capture `find . -name '*.gcda' | xargs -n1 dirname | xargs -n1 echo -n ' --directory'` --output-file=coverage.info"
    COMMAND lcov --remove coverage.info "*gtest*" --no-external --output-file=coverage1.info
    COMMAND lcov --extract coverage1.info "*/nvchunk*" --no-external --output-file=nvchunk.info
    COMMAND genhtml nvchunk.info --output-directory=res
    COMMAND echo "Generating Gcov html files in ${CMAKE_BINARY_DIR}/res"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS nvchunk_test
    VERBATIM
)
