/**
 * @author Fred Chen
 * @email fred.chen@live.com
 * @create date 2022-03-04 10:19:09
 * @modify date 2022-03-04 10:19:09
 * @desc cross platform pmem functions
 */

#include <config.h>
#include <sys/mman.h>       // no PMDK

#ifdef HAVE_LIBPMEM_H
#include <libpmem.h>        // PMDK present
#else
int pmem_is_pmem(const void * addr, size_t len) {
    return 0;
}
void pmem_persist(const void * addr, size_t len) {
    ::msync(addr, len, MS_SYNC);
}
void * pmem_map_file(const char * path, size_t len, int flags, mode_t mode, size_t *mapped_lenp, int * is_pmemp) {
    void *p = ::mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    *mapped_lenp = p ? len : 0;
    *is_pmem = 0;
    return p;
}
int pmem_unmap(void * addr, size_t len) {
    return munmap(addr, len);
}
#endif // HAVE_LIBPMEM_H
