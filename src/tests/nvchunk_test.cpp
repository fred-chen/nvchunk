#define UNDER_GTEST

#include "nvchunk.hpp"
#include <fstream>
#include <sstream>
#include "flog.hpp"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std;
using namespace NVCHUNK;

using FLOG::INFO;
using FLOG::WARN;
using FLOG::ERROR;

static const std::string str_pmem_mntpt = "/pmem/";

TEST_CASE("nvchunkTest0", "[usage]") {

// file based chunk
#ifdef HAVE_LIBPMEM_H
    string path = str_pmem_mntpt + "DEV1";
#else
    string path = "/tmp/DEV1";
#endif

    // create and map a memory based chunk
    nvchunk *pc_m = NVM::instance().openChunk("chunk_m", "", 0, MB * 1);

    // create and map a file based chunk
    nvchunk *pc_f = NVM::instance().openChunk("chunk_f", path, 0, MB * 1);

    // now you can use pc->va() as if it's a regular pointer
    strcpy( (char*) pc_m->va(), "Hello NVM");
    memcpy(pc_f->va(), pc_m->va(), strlen("Hello NVM")+1);

    // nvchunk::flush() to flush data onto disk or NVM
    pc_f->flush();

    // nvchunk::flush() always return -1 and set errno 
    // if a chunk is memory backed 
    REQUIRE(-1 == pc_m->flush());

    // after flush the content to backing file
    // data should be persistent
    int fd = ::open(path.c_str(), O_RDWR);
    char buf[20] = {0};
    ::read(fd, buf, strlen("Hello NVM")+1);
    REQUIRE(std::string(buf) == "Hello NVM");

    // a chunk can be converted (or say mapped) to any structures
    // with a mapper object for easy access.
    struct SA {
        char age;
        char name[10];
    } a = { 13, "abcd1234" };          // this is a sample structure
    
    auto ma = pc_f->getmapper<SA>();   // get a mapper for SA type records
    ma[0] = a;                         // you can easily use ma as an 
                                       // SA[] array to access its elements
    ma.flush( & (ma[0]) );             // flush a single element to disk
    NVM::instance().clear();           // close all devices and unmap all chunks

    REQUIRE(0 == NVM::instance().nchunks());
    REQUIRE(0 == NVM::instance().ndevs());

    // reopen the nv_dev and map it to a chunk
    pc_f = NVM::instance().openChunk("chunk_f", path, 0, 0);
    auto mb = pc_f->getmapper<SA>();   // get the new mapper
    REQUIRE(mb[0].age == a.age);       // check data persistency after reopen
    REQUIRE(std::string(mb[0].name) == a.name);
    
    unlink(path.c_str());
}


#ifdef HAVE_LIBPMEM_H
TEST_CASE("nvchunkTest1", "[mntpt]") {
    ofstream ofs(str_pmem_mntpt+"nvchunk.data");
    ifstream ifs(str_pmem_mntpt+"nvchunk.data");
    string s("try pmemlib");
    stringstream sb;
    char buf[100];

    ofs << "try pmemlib" << endl;
    ifs.getline(buf, 99);

    REQUIRE_THAT(buf, Catch::Matchers::Equals(s));
}
#endif

TEST_CASE("nvchunkTest2", "[nv_filedev]") {
    string path;
    nv_dev* dev = nullptr;

#ifdef HAVE_LIBPMEM_H
    /* testing on a real NVM device */
    path = str_pmem_mntpt + "dev1";
    
    unlink(path.c_str());

    /* creating new file without size will raise an exception */
    // EXPECT_THROW(new nv_filedev(path, 0), nv_exception);

    /* creating new file on a dax file system*/
    dev = new nv_filedev(path, MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(dev->is_pmem());
    REQUIRE(0 == dev->flush());
    delete dev;
    unlink(path.c_str());
#endif

    /* creating new file on a regular file system*/
    path = "/tmp/dev1";
    unlink(path.c_str());
    dev = new nv_filedev(path, MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(!dev->is_pmem());
    REQUIRE(0 == dev->flush());
    delete dev;

    /* open existing file */
    dev = new nv_filedev(path, 0);
    REQUIRE(dev);
    REQUIRE(dev->va());
    REQUIRE(dev->size() == MB * 10);
    delete dev;
    unlink(path.c_str());
}

TEST_CASE("nvchunkTest3", "[nv_memdev]") {

    /* creating new mapping without size will raise an exception */
    // EXPECT_THROW(new nv_memdev(0), nv_exception);

    /* creating new mapping */
    nv_dev* dev = new nv_memdev(MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(!dev->is_pmem());
    REQUIRE(dev->flush() == -1);    // flush() does nothing 
                                    // but return -1 for memory based device
    delete dev;

}

TEST_CASE("nvchunkTest4", "[nv_dev]") {
    string path;
    nv_dev* dev;

    /* file based */
#ifdef HAVE_LIBPMEM_H
    path = str_pmem_mntpt + "dev1";
    dev = nv_dev::open(path, MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(dev->is_pmem());
    delete dev;
    unlink(path.c_str());
#endif

    path = "/tmp/dev1";
    dev = nv_dev::open(path, MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(!dev->is_pmem());
    delete dev;
    unlink(path.c_str());

    REQUIRE(nullptr ==  nv_dev::open(path, 0));

    /* mem based */
    dev = nv_dev::open("", MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(!dev->is_pmem());
}

TEST_CASE("nvchunkTest5", "[nvchunk]") {
    nv_dev* dev = nv_dev::open("", MB * 10);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->va() != nullptr);
    REQUIRE(!dev->is_pmem());

    /* map a region on the backing device */
    nvchunk* pc = new nvchunk("memchunk1", dev, 4, dev->size() - 4);
    REQUIRE(pc->va() == (char*)dev->va() + 4);
    REQUIRE(pc->size() == dev->size() - 4);
    delete pc;

    /* map the whole device */
    pc = new nvchunk("memchunk1", dev, 0, 0);
    REQUIRE(pc->va() == dev->va());
    REQUIRE(pc->size() == dev->size());    
    
    delete pc;
}

TEST_CASE("nvchunkTest6", "[NVM]") {

    // file based chunk
#ifdef HAVE_LIBPMEM_H
    string path = str_pmem_mntpt + "DEV1";
#else
    string path = "/tmp/DEV1";
#endif

    unlink(path.c_str());
    NVM::instance().clear();

    // open a none existing file without specifying a size returns nullptr
    REQUIRE(nullptr == NVM::instance().openChunk("chunk1", path, 0, 0));

    // open a none existing file with a size returns a nvchunk object
    nvchunk* pc = NVM::instance().openChunk("chunk2", path, 32, MB * 13);
    REQUIRE(nullptr != pc);
    REQUIRE(pc->size() == pc->_pDev->size() - 32);

    struct stat st;
    REQUIRE(0 == stat(path.c_str(), &st));
    REQUIRE(st.st_size == MB * 13+32);
    REQUIRE((size_t)st.st_size == pc->_pDev->size());

    pc->flush();

    /* open the same chunk for the 2nd time should return existing chunk */
    nvchunk* pc1 = NVM::instance().openChunk("chunk2", path, 0, 0);
    REQUIRE(pc == pc1);

    /* create a new chunk on an opened device should use existing nv_dev device */
    nvchunk* pc2 = NVM::instance().openChunk("chunk3", path, 2, MB * 10);
    REQUIRE(pc2 != nullptr);
    REQUIRE(pc->_pDev == pc2->_pDev);

    /* expect nullptr if dev is null */
    REQUIRE(nullptr == NVM::instance().mapChunk("chunk4", nullptr, 0, 0));

    /* mapping is shared if they have a same backing device */
    nvchunk::mapper<char> chars(pc);
    chars[0] = 'F';
    REQUIRE('F' == *(char*)pc->va());
    REQUIRE('F' == *((char*)pc2->va()+30));
    auto chars2 = pc2->getmapper<char>();
    REQUIRE('F' == chars2[30]);
    REQUIRE('F' == (char)(((char*)pc2->_pDev->va())[32]));
    chars[pc->size()-1] = 'R';
    REQUIRE('R' == (char)(((char*)pc->_pDev->va())[pc->_pDev->size()-1]));

    /* unmapping a chunk */
    size_t count = NVM::instance().nchunks();
    NVM::instance().unmapChunk("chunk2");
    REQUIRE(NVM::instance().mChunks.size() == count-1);
    
    /* close a dev */
    count = NVM::instance().ndevs();
    NVM::instance().closeDev(path.c_str());
    REQUIRE(NVM::instance().mDevs.size() == count-1);
    REQUIRE(NVM::instance().mDevs.size() == 0);
}
