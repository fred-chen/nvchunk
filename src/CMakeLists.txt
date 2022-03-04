
include_directories("nvchunk")
include_directories("public")

add_subdirectory(nvchunk)
add_subdirectory(public)

include(CTest)
enable_testing()
add_subdirectory(tests)
