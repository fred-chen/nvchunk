add_library(nvchunk SHARED nvchunk.cpp)

IF(NOT HAVE_LIBPMEM_H)
MESSAGE( STATUS "NO LIBPMEM" )
ELSE()
target_link_libraries(nvchunk pmem)
ENDIF()

