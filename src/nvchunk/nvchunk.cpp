#include "nvchunk.hpp"

/**
 * @brief open an nvm device, use memory to simulate if path is ""
 *        create a new file if path doesn't exist
 * 
 * @param name name of this chunk
 * @param path path of the nvm backing file
 * @param size length in bytes
 * 
 * @return a pointer to an nv_dev object, nullptr if fail to open
 */
nv_dev* nv_dev::open(const string & name, size_t size)
{
    struct stat st_buf;
    nv_dev* pDev;
    
    try {
        if ( name == "" ) {
            pDev = new nv_memdev(size);
        }
        else {
            /*
             * file exists, choose the corresponding object type
             * nv_filedev is for general usage
             * there could be optimal implementation for specific file types
             *   S_ISCHR(st_buf.st_mode) devdax device (nvdimm or optane)
             *   S_ISBLK(st_buf.st_mode) block device (nvme ssd)
             *   S_ISREG(st_buf.st_mode) file on a fsdax file system or a regular file system
             */
            pDev = new nv_filedev(name, size);
        }
    }
    catch (nv_exception e) {
        LOG(ERROR) << e.what() << std::endl;
        return nullptr;
    }
    return pDev;
}
