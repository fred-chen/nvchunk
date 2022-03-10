add_library(nvchunk STATIC nvchunk.cpp)

IF(NOT HAVE_LIBPMEM_H)
MESSAGE( STATUS "NO LIBPMEM" )
ELSE()
target_link_libraries(nvchunk pmem)
ENDIF()

# add_custom_command(
#     TARGET nvchunk
#     POST_BUILD
#     COMMAND objcopy --only-keep-debug libnvchunk.so libnvchunk.debug
#     COMMAND strip --strip-debug libnvchunk.so
#     COMMAND objcopy --add-gnu-debuglink=libnvchunk.debug libnvchunk.so
# )
