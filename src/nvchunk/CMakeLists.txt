add_library(nvchunk STATIC nvchunk.cpp)

IF(NOT HAVE_LIBPMEM_H)
MESSAGE( STATUS "NO LIBPMEM" )
ELSE()
target_link_libraries(nvchunk pmem)
ENDIF()

