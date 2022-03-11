/**
 * @author Fred Chen
 * @email fred.chen@live.com
 * @create date 2022-03-01 15:00:41
 * @modify date 2022-03-01 15:00:41
 * @desc nvchunk represents a contigous region on an nvm device
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
#include <random>
#include "cpmem.hpp"
#include "flog.hpp"

namespace NVCHUNK {

#define UNUSED(expr) do { (void)(expr); } while (0);

#ifdef  UNDER_GTEST
#define GTEST_ONLY(expr) expr
#else
#define GTEST_ONLY(expr)
#endif

#define KB (0x1<<10)
#define MB (0x1<<20)
#define GB (0x1<<30)

template <class T>
class Singleton
{
public:
    static T& instance()
    {
        static T inst;
        return inst;
    }

    Singleton(Singleton<T> const &)         = delete;
    void operator=(Singleton<T> const &)    = delete;
protected:
    Singleton() = default;
    ~Singleton() = default;
};

class nv_exception : public std::exception {
    int   _errno;
    std::string _errstr;
    std::string _whatmsg;
public:
    nv_exception(std::string msg) noexcept : _errno(errno), _errstr(std::strerror(errno))
    {
        _whatmsg = msg + "(errno(" + std::to_string(errno) + "): " + _errstr + ")";
    }
    const char* what() const noexcept {
        return _whatmsg.c_str();
    }
};

inline std::string uuid() {
    static std::random_device dev;
    static std::mt19937 rng(dev());

    std::uniform_int_distribution<int> dist(0, 15);

    const char *v = "0123456789abcdef";
    const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

    std::string res;
    for (int i = 0; i < 16; i++) {
        if (dash[i]) res += "-";
        res += v[dist(rng)];
        res += v[dist(rng)];
    }
    return res;
}

using std::string;
using FLOG::LOG;
using FLOG::ERROR;
using FLOG::INFO;
using FLOG::WARN;

/**
 * @brief an nv_dev object represents a mapped NVM device
 *        an NVM device can be either:
 *        1. a file based device
 *           a) a file on a regular file system
 *           b) a file on a dax file system (backed by fsdax NVDIMM or Optane)
 *           c) a devdax device (e.g. /dev/dax0.0)
 *        2. a memory based device
 */
class nv_dev {
protected:
    string    mName;             // name of this backing device
    size_t    mSize;             // size of dev
    void*     mVA;               // virtual address mapped
    int       mIsPmem;           // is the backing device an NVM device?
public:
    size_t         size () const { return mSize; }
    const string & name () const { return mName; }
    void*          va   () const { return mVA;   }

    static nv_dev* open(const string & name, size_t size);
    virtual bool close() = 0;

    nv_dev(string name = "", size_t size=0) 
        : mName(name),mSize(size),mVA(nullptr),mIsPmem(false) {}
    virtual ~nv_dev() {}

    /**
     * @brief persist the data of the whole range
     * 
     * @return int 0 if succ, else -1 with errno set
     */
    virtual int flush(void* addr = nullptr, size_t size = 0) = 0;

    virtual bool is_pmem(bool retest = false) {
        return retest ? pmem_is_pmem(mVA, mSize) : mIsPmem;
    }

};

/**
 * @brief an nv_filedev is an nv_dev backed by a file
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

        if((mVA = (void*)pmem_map_file(mName.c_str(), size, flags, 0666, &mapped_len, &is_pmem)) == nullptr) {
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
    virtual int flush(void* addr = 0, size_t size = 0) override { 
        int rt = 0;
        if( mIsPmem ) {
            (addr && size) ? pmem_persist(addr, size) : 
                             pmem_persist(mVA, mSize);
        }
        else {
            rt = (addr && size) ? pmem_msync(addr, size) : 
                                  pmem_msync(mVA, mSize);
        }
        return rt;
    }
    
    const string & path() {
        return mName;
    }

    virtual ~nv_filedev() { close(); }

};

/**
 * @brief an nv_memdev is an nv_dev backed by memory
 * 
 */
class nv_memdev : public nv_dev {
public:
    nv_memdev(size_t size) : nv_dev("", size) {
        /* use anonymous mmap to create memory based mapping */
        if(!size) {
            throw nv_exception("creating memory based mapping with zero size.");
        }

        mVA = (void*) ::mmap(NULL, size, PROT_READ | PROT_WRITE, 
                                MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
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

    virtual int flush(void* addr, size_t size) override {
        UNUSED(addr); UNUSED(size);
        /* no action taken for memory based device */
        errno = EINVAL;
        return -1;
    }

    virtual ~nv_memdev() { close(); }
};

/**
 * @brief an nvchunk object represents a contiguous region on an NVM device
 *        that was mapped to a virtual address 
 * 
 */
class nvchunk {
private:
GTEST_ONLY(public:)
    string    mName;         // name of this chunk
    uint64_t  mFlags;        // flags of this chunk
    nv_dev*   _pDev;         // the backing device
    void*     mVA;           // starting address of this chunk
    size_t    mSize;         // size of this chunk
public:
    const string& name() const { return mName; }
    void*    va () const { return mVA; }
    size_t size () const { return mSize; }
    bool is_nvm () const { return _pDev->is_pmem(); }
    int flush()                           { return _pDev->flush(mVA, mSize); }
    int flush( void * addr, size_t size ) { return _pDev->flush(addr, size); }

    nvchunk(const string & name, nv_dev* dev, off_t off=0, size_t size=0)
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

    template <typename T>
    class mapper {
    private:
        nvchunk*  mParent;
        T*        mT;
        size_t    mNumElements;
    public:
        mapper(nvchunk* parent) : mParent(parent), mNumElements(0) {
            mNumElements = mParent->size() / sizeof(T);
            mT = (T*)mParent->va();
        }
        T & operator [] (size_t index) {
            return mT [index];
        }

        void flush( T *item ) {
            mParent->flush( (void*) item, sizeof(T) );
        }

        void flush( size_t index ) {
            mParent->flush( (void*) & mT[index], sizeof(T) );
        }

        T* operator->() {
            return mT;
        }
    };

    template <typename T>
    mapper<T> getmapper() {
        return mapper<T>(this);
    }
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

    /**
     * @brief create or open a new nv_dev object and add it to mDevs
     *        if path is "", create a private memory based nv_dev
     *        if path isn't "", create a file based nv_dev
     *        if the file specified by path doesn't exist, create a new file
     * 
     * @param path a file path, or "" if the device is memory based
     * @param size size of the nv_dev
     * @return nv_dev* the address of nv_dev
     */
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
     * @brief close an nv_dev
     * 
     * @param name name of the nv_dev to be closed
     */
    void closeDev( const string & name ) {
        nv_dev* pd = getDev(name);
        if(pd) {
            pd->close();
            for( auto itr = mDevs.begin(); itr != mDevs.end(); itr++ ) {
                if((*itr)->name() == name) {
                    mDevs.erase(itr);
                    break;
                }
            }
        }
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
    nvchunk* mapChunk(const string & name, nv_dev* dev, 
                        off_t off=0, size_t size=0)
    {
        nvchunk* pc;

        try {
            pc = new nvchunk(name, dev, off, size);
        }
        catch (nv_exception & e) {
            LOG(ERROR) << e.what() << std::endl;
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
     * @brief Get the nv_dev object in mDevs according to name
     * 
     * @param name the name of nv_dev
     * @return nv_dev* a pointer to nv_dev if found, else nullptr
     */
    nv_dev* getDev(const string & name) {
        for( nv_dev *pd : mDevs ) {
            if( pd->name() == name ) {
                return pd;
            }
        }
        return nullptr;
    }

    /**
     * @brief create an nvm chunk by path, add the chunk pointer to mChunks.
     *        if a chunk of the name in arg list already exists, return the 
     *        existing pointer
     *        otherwise, create a new chunk according the path argument
     *        a new nv_dev object may be created if it's not currently in mDevs
     * 
     * @param name   name of the chunk
     * @param path   path of backing device, a file path of an nvm device or a 
     *               regular file, 
     *               memory simulated backing device if path==""
     * @param offset offset within the backing device
     * @param size   size of the chunk, size must be greater or equal to the 
     *               backing device size
     *               if size == 0, use whole backing device as chunk
     * @return       a pointer to nvchunk object if open success
     * @return       nullptr if failed to open
     */
    nvchunk* openChunk(string name, const string & path = "", off_t offset = 0, size_t size = 0) 
    {
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

    void unmapChunk( void * va )
    {
        for(auto it = mChunks.begin(); it != mChunks.end(); it++) {
            if((*it)->va() == va) {
                delete (*it);
                mChunks.erase(it);
                break;
            }
        }
    }

    int nchunks() {
        return mChunks.size();
    }
    int ndevs() {
        return mDevs.size();
    }

    /**
     * @brief close all nv_devs in mDevs and unmap all chunks in mChunks
     * 
     */
    void clear() {
        for( auto pc : mChunks ) {
            delete pc;
        }
        for( auto pd: mDevs ) {
            pd->close();
            delete pd;
        }
        mChunks.clear();
        mDevs.clear();
    }
};

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
inline nv_dev* nv_dev::open(const string & name, size_t size)
{
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
    catch (nv_exception & e) {
        LOG(ERROR) << e.what() << std::endl;
        return nullptr;
    }
    return pDev;
}


} // namespace NVCHUNK