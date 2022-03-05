/**
 * @author Fred Chen
 * @email fred.chen@live.com
 * @create date 2022-03-04 10:19:09
 * @modify date 2022-03-04 10:19:09
 * @desc cross platform pmem functions
 */

#include <config.hpp>
#include <sys/mman.h>       // no PMDK
#include "public.hpp"

#ifdef HAVE_LIBPMEM_H
#include <libpmem.h>        // PMDK present
#else
/*
 * the environment doesn't have PMDK library
 * we must provide alternative implementation
 * for pmem functions.
 */
# define PMEM_FILE_CREATE O_CREAT|O_RDWR

int pmem_is_pmem(const void * addr, size_t len) {
    return 0;
}

void pmem_persist(void * addr, size_t len) {
    ::msync(addr, len, MS_SYNC);
}

int pmem_msync(void * addr, size_t len) {
    return ::msync(addr, len, MS_SYNC);
}

void * pmem_map_file(const char * path, size_t len, int flags, mode_t mode, size_t *mapped_lenp, int * is_pmemp) {
    // ERR("pmem_map_file: len=%d path=%s flags=0x%x mode=0%o", len, path, flags, mode);

    struct stat st;
    int fd;

    flags = flags | O_RDWR;
    fd = ::open(path, flags, mode);
    if(fd == -1) { 
        ERR("failed to open %s", path);
        return nullptr; 
    }
    if( flags & O_CREAT ) {
        if( ftruncate(fd, len) == -1 ) {
            ERR("failed to ftruncate %s to %d bytes.", path, len);
            ::close(fd);
            return nullptr;
        }
    }
    if(!len) {
        if( fstat(fd, &st) == -1 ) {
            ERR("failed to stat %s.", path);
            ::close(fd);
            return nullptr;
        }
        len = st.st_size;
    }

    void *p = ::mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    *mapped_lenp = p ? len : 0;
    *is_pmemp = 0;
    if(p == MAP_FAILED) {
        ERR("mmap failed. fd=%d len=%d path=%s", fd, len, path);
        return nullptr;
    }
    ::close(fd);
    return p;
}
int pmem_unmap(void * addr, size_t len) {
    return ::munmap(addr, len);
}
#endif // HAVE_LIBPMEM_H
