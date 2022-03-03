
add_executable(nvchunk_test nvchunk_test.cpp)                      # 编译主文件
target_link_libraries(nvchunk_test nvchunk gtest gtest_main)       # 主文件要依赖于 gtest.a 库


add_test(NAME nvchunk_test COMMAND nvchunk_test)

add_custom_target(gcov ALL
    DEPENDS nvchunk_test gcov.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
)

add_custom_command(OUTPUT gcov.txt
    COMMAND ctest -V
    COMMAND eval "lcov --capture `find . -name '*.gcda' | xargs -n1 dirname | xargs -n1 echo -n ' --directory'` --output-file=coverage.info"
    COMMAND lcov --extract coverage.info "*nvchunk*" --output-file=nvchunk.info
    COMMAND genhtml nvchunk.info --output-directory=res
    COMMAND echo "Generating Gcov html files in ${CMAKE_BINARY_DIR}/res"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
)

