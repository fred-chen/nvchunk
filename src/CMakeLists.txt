
# platform detection
INCLUDE (CheckIncludeFiles)
CHECK_INCLUDE_FILES (libpmem.h HAVE_LIBPMEM_H)

include_directories("nvchunk")

add_subdirectory(nvchunk)
add_subdirectory(tests)