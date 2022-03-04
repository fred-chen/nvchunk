/**
 * @author Fred Chen
 * @email fred.chen@live.com
 * @create date 2022-03-01 15:00:41
 * @modify date 2022-03-01 15:00:41
 * @desc nvchunk represents a continious region on an nvm device
 */


#include <vector>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include "singleton.hpp"
#include "public.hpp"
#include "cpmem.hpp"

using std::string;


/**
 * @brief an nv_dev object represents a mapped NVM device
 *        an NVM device can be either:
 *        1. a file based device
 *           a) a file on a regular file system
 *           b) a file on a dax file system ( backed by fsdax NVDIMM or Optane )
 *           c) a devdax device (eg. /dev/dax0.0)
 *        2. a memory based device
 */
class nv_dev {
protected:
    string    mName;             // name of this backing device
    size_t    mSize;             // size of dev
    char*     mVA;               // virtual address mapped
    int       mIsPmem;           // is the backing device an NVM device?
public:
    size_t          size()  { return mSize; }
    const string&   name()  { return mName; }
    char*           va()    { return mVA;   }

    static nv_dev* open(const string & name, size_t size);
    virtual bool close() = 0;

    nv_dev(string name = "", size_t size=0) 
        : mName(name),mSize(size),mVA(nullptr),mIsPmem(false) 
    {
    }
    virtual ~nv_dev() {}

    /**
     * @brief persist the data of the whole range
     * 
     * @return int 0 if succ, else -1 with errno set
     */
    virtual int flush(char* addr = nullptr, size_t size = 0) = 0;

    virtual bool is_pmem(bool retest = false) {
        return retest ? pmem_is_pmem(mVA, mSize) : mIsPmem;
    }

};

/**
 * @brief a nv_filedev is an nv_dev backed by a file
 *        the backing file can be a file on dax file system
 *        or a DAX raw device or a file on a regular file system
 * 
 */
class nv_filedev : public nv_dev {
public:
    /**
     * @brief Construct a new nvm device object with file type backing device
     *        - Open if backing device file already exists
     *        - Create if backing device file does not exist
     * 
     * @param name the path to nvm device file
     * @param size size of the backing file for creation
     */
    nv_filedev(string path, size_t size=0) : nv_dev(path, size)
    {
        struct stat st;
        size_t mapped_len = 0;
        int    is_pmem = 0;
        int    flags = 0;

        if( stat(mName.c_str(), &st) != 0 ) {
            /* backing file doesn't not exist, create and map with given size */
            if(!size) {
                throw nv_exception("new file with zero size.");
            }
            flags |= PMEM_FILE_CREATE;
            mSize = size;
        }
        else {
            /* backing file exists, map whole file size */
            mSize = st.st_size;
            size  = 0;
        }

        if((mVA = (char*)pmem_map_file(mName.c_str(), size, flags, 0666, &mapped_len, &is_pmem)) == nullptr) {
            throw nv_exception("failed to map device.");
        }

        if(mapped_len != mSize) {
            throw nv_exception("partial mapped device.");
        }

        mIsPmem = pmem_is_pmem(mVA, mSize);
    }

    virtual bool close() override {
        if(mVA && mSize && pmem_unmap(mVA, mSize)) {
            return false;
        }
        mVA   = nullptr;
        mSize = 0;
        return true;
    }

    /**
     * @brief flush data onto persistent memory
     *        calling pmem_persist for NVM devices (NVDIMM, Optane)
     *        calling pmem_msync for regular file based devices
     * 
     * @param addr the starting address to flush
     * @param size size of data to flush
     * @return int 0 if succ, else -1 and errno tells why
     */
    virtual int flush(char* addr = 0, size_t size = 0) override { 
        int rt = 0;
        if( mIsPmem ) {
            (addr && size) ? pmem_persist(addr, size) : pmem_persist(mVA, mSize); 
        }
        else {
            rt = (addr && size) ? pmem_msync(addr, size) : pmem_msync(mVA, mSize);
        }
        return rt;
    }
    
    const string & path() {
        return mName;
    }

    virtual ~nv_filedev() { close(); }

};

/**
 * @brief a nv_memdev is an nv_dev backed by memory
 *        the code will try to use huge pages to map a big chunk of memory
 * 
 */
class nv_memdev : public nv_dev {
public:
    nv_memdev(size_t size) : nv_dev("", size) {
        /* use anonymous mmap to create memory based mapping */
        if(!size) {
            throw nv_exception("creating memory based mapping with zero size.");
        }

        mVA = (char*) ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_HUGETLB, -1, 0);
        if (mVA == MAP_FAILED) {
            throw nv_exception("failed to mmap /dev/zero.");
        }
        mName = uuid();
    }

    virtual bool close() override {
        if(mVA && mSize && ::munmap(mVA, mSize)) {
            return false;
        }
        mVA   = nullptr;
        mSize = 0;
        return true;
    }

    virtual int flush(char* addr, size_t size) override { 
        /* no action taken for memory based device */
        return 0;
    }

    virtual ~nv_memdev() { close(); }
};

/**
 * @brief a nvmchunk object represents a contiguous region on an NVM device
 *        that mapped to a virtual address 
 * 
 */
class nvchunk {
private:
GTEST_ONLY(public:)
    char*     mVA;           // starting address of this chunk
    size_t    mSize;         // size of this chunk
    uint64_t  mFlags;        // flags of this chunk
    nv_dev*   _pDev;         // the backing device
    string    mName;         // name of this chunk
public:
    const string& name() {
        return mName;
    }
    char* va() {
        return mVA;
    }
    size_t size() {
        return mSize;
    }
    int flush() {
       return _pDev->flush(mVA, mSize);
    }
    bool is_nvm() {
        return _pDev->is_pmem();
    }

    nvchunk(const string & name, nv_dev* dev, off64_t off=0, size_t size=0)
        : mName(name), mFlags(0), _pDev(dev), mVA(nullptr), mSize(size) 
    {
        if(!_pDev) {
            throw nv_exception("null dev.");
        }
        mVA = (char*)_pDev->va() + off;
        if(!mSize) {
            mSize = _pDev->size();
        }
    }

    class mapper {

    };
};


/**
 * @brief nv manager, manages multiple chunks on NVM devices
 * 
 */
class NVM : public Singleton<NVM>
{
private:
GTEST_ONLY(public:)
    std::vector<nvchunk*> mChunks;        // NVM chunks
    std::vector<nv_dev*>  mDevs;          // NVM devices
public:
    nv_dev* openDev(const string & path, size_t size=0) {
        if( path != "" ) {
            // find existing backing dev
            for( nv_dev* pd : mDevs ) {
                if(pd->name() == path) {
                    return pd;
                }
            }
        }
        // no existing dev, open a new dev
        nv_dev* pd = nv_dev::open(path, size);
        if( pd ) {
            mDevs.push_back(pd);
        }
        return pd;
    }

    /**
     * @brief create new chunk and map it on an existing nv_dev
     * 
     * @param name        name of the chunk
     * @param dev         nv_dev backing device
     * @param off         offset of the chunk from beginning of dev
     * @param size        size of the chunk
     * @return nvchunk*   a pointer to the newly created nvchunk
     */
    nvchunk* mapChunk(const string & name, nv_dev* dev, off64_t off=0, size_t size=0) {
        nvchunk* pc;

        try {
            pc = new nvchunk(name, dev, off, size);
        }
        catch (nv_exception e) {
            ERR(e.what());
            return nullptr;
        }
        mChunks.push_back(pc);
        return pc;
    }

    /**
     * @brief Get the existing nvchunk pointer of the given name
     * 
     * @param name 
     * @return nvchunk* 
     */
    nvchunk* getChunk(const string & name) {
        // check existing chunks with the same name
        // return existing chunk if found
        for(nvchunk* pc : mChunks ) {
            if(pc->name() == name) {
                return pc;
            }
        }
        return nullptr;
    }

    /**
     * @brief create an nvm chunk by path, add the chunk pointer to mChunks.
     *        if a chunk of the name in arg list already exists, return the existing pointer
     *        otherwise, create a new chunk according the path argument
     *        a new nv_dev object may be created if it's not currently in mDevs
     * 
     * @param name   name of the chunk
     * @param path   path of backing device, a file path of an nvm device or a regular file, 
     *               memory simulated backing device if path==""
     * @param offset offset within the backing device
     * @param size   size of the chunk, size must be greater or equal to the backing device size
     *               if size == 0, use whole backing device as chunk
     * @return a pointer to nvchunk object if open success
     * @return nullptr if failed to open
     */
    nvchunk* openChunk(string name, const string & path = "", off64_t offset = 0, size_t size = 0) 
    {
        int rt;

        // check existing chunks with the same name
        // return existing chunk if found
        nvchunk *pc = getChunk(name);
        if(pc)
            return pc;
        // the chunk doesn't exist, create a new chunk
        nv_dev* pDev = openDev(path, size+offset);
        if(!pDev)
            return nullptr;
        pc = mapChunk(name, pDev, offset, size);

        return pc;
    }
    
    void unmapChunk(const string & name)
    {
        for(auto it = mChunks.begin(); it != mChunks.end(); it++) {
            if((*it)->name() == name) {
                delete (*it);
                mChunks.erase(it);
                break;
            }
        }
    }

    int nchunks() {
        return mChunks.size();
    }
};