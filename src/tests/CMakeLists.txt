
add_executable(nvchunk_test nvchunk_test.cpp)              # 编译主文件
target_link_libraries(nvchunk_test gtest gtest_main)       # 主文件要依赖于 gtest.a 库


add_test(NAME nvchunk_test COMMAND nvchunk_test)
