#define UNDER_GTEST

#include "nvchunk.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <sstream>

using namespace std;

#define PMEM_MNTPT "/pmem/"

struct nvchunkTest : public testing::Test {
    ofstream ofs;
    ifstream ifs;
    char content_pattern[8] = "abcdcba";
    char *env_pmem_mntpt = std::getenv("PMEM_MNTPT");
    string str_pmem_mntpt = env_pmem_mntpt ? env_pmem_mntpt : PMEM_MNTPT;

    void SetUp() {
        ofs.open(PMEM_MNTPT "libpmem_test");
        ifs.open(PMEM_MNTPT "libpmem_test");
    }
 
    void TearDown() { 
        ofs.close();
        ifs.close();
    }
};

TEST_F(nvchunkTest, usage) {

// file based chunk
#ifdef HAVE_LIBPMEM_H
    string path = str_pmem_mntpt + "DEV1";
#else
    string path = "/tmp/DEV1";
#endif

    // create and map a memory based chunk
    nvchunk *pc_m = NVM::instance().openChunk("chunk_m", "", 0, MB(1));

    // create and map a file based chunk
    nvchunk *pc_f = NVM::instance().openChunk("chunk_f", path, 0, MB(1));

    // now you can use pc->va() as if it's a regular pointer
    strcpy( (char*) pc_m->va(), "Hello NVM");
    memcpy(pc_f->va(), pc_m->va(), strlen("Hello NVM")+1);

    // nvchunk::flush() to flush data onto disk or NVM
    pc_f->flush();

    // nvchunk::flush() always return -1 and set errno 
    // if a chunk is memory backed 
    EXPECT_EQ(-1, pc_m->flush());

    // after flush the content to backing file
    // data should be persistent
    int fd = ::open(path.c_str(), O_RDWR);
    char buf[20] = {0};
    ::read(fd, buf, strlen("Hello NVM")+1);
    EXPECT_STREQ("Hello NVM", buf);

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

    EXPECT_EQ(0, NVM::instance().nchunks());
    EXPECT_EQ(0, NVM::instance().ndevs());

    // reopen the nv_dev and map it to a chunk
    pc_f = NVM::instance().openChunk("chunk_f", path, 0, 0);
    auto mb = pc_f->getmapper<SA>();   // get the new mapper
    EXPECT_EQ(mb[0].age, a.age);       // check data persistency after reopen
    EXPECT_STREQ(mb[0].name, a.name);
    
    unlink(path.c_str());
}


#ifdef HAVE_LIBPMEM_H
TEST_F(nvchunkTest, mntpt) {
    string s("try pmemlib");
    stringstream sb;
    char buf[100];

    ofs << "try pmemlib" << endl;
    ifs.getline(buf, 99);

    EXPECT_STREQ(buf, s.c_str());
}
#endif

TEST_F(nvchunkTest, nv_filedev) {
    string path;
    nv_dev* dev = nullptr;

#ifdef HAVE_LIBPMEM_H
    /* testing on a real NVM device */
    path = str_pmem_mntpt + "dev1";
    
    unlink(path.c_str());

    /* creating new file without size will raise an exception */
    EXPECT_THROW(new nv_filedev(path, 0), nv_exception);

    /* creating new file on a dax file system*/
    dev = new nv_filedev(path, MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_TRUE(dev->is_pmem());
    EXPECT_EQ(0, dev->flush());
    delete dev;
    unlink(path.c_str());
#endif

    /* creating new file on a regular file system*/
    path = "/tmp/dev1";
    unlink(path.c_str());
    dev = new nv_filedev(path, MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_FALSE(dev->is_pmem());
    EXPECT_EQ(0, dev->flush());
    delete dev;

    /* open existing file */
    dev = new nv_filedev(path, 0);
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_EQ(dev->size(), MB(10));
    delete dev;
    unlink(path.c_str());
}

TEST_F(nvchunkTest, nv_memdev) {

    /* creating new mapping without size will raise an exception */
    EXPECT_THROW(new nv_memdev(0), nv_exception);

    /* creating new mapping */
    nv_dev* dev = new nv_memdev(MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_FALSE(dev->is_pmem());
    EXPECT_EQ(dev->flush(), -1);    // flush() does nothing 
                                   // but return -1 for memory based device
    delete dev;

}

TEST_F(nvchunkTest, nv_dev) {
    string path;
    nv_dev* dev;

    /* file based */
#ifdef HAVE_LIBPMEM_H
    path = str_pmem_mntpt + "dev1";
    dev = nv_dev::open(path, MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_TRUE(dev->is_pmem());
    delete dev;
    unlink(path.c_str());
#endif

    path = "/tmp/dev1";
    dev = nv_dev::open(path, MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_FALSE(dev->is_pmem());
    delete dev;
    unlink(path.c_str());

    LOG(ERROR) << "Expecting nullptr ..." << std::endl;
    EXPECT_EQ(nullptr, nv_dev::open(path, 0));
    LOG(ERROR) << "END Expecting nullptr ..." << std::endl;

    /* mem based */
    dev = nv_dev::open("", MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_FALSE(dev->is_pmem());
}

TEST_F(nvchunkTest, nvchunk) {
    nv_dev* dev = nv_dev::open("", MB(10));
    EXPECT_NE(dev, nullptr);
    EXPECT_NE(dev->va(), nullptr);
    EXPECT_FALSE(dev->is_pmem());

    /* map a region on the backing device */
    nvchunk* pc = new nvchunk("memchunk1", dev, 4, dev->size() - 4);
    EXPECT_EQ(pc->va(), (char*)dev->va() + 4);
    EXPECT_EQ(pc->size(), dev->size() - 4);
    delete pc;

    /* map the whole device */
    pc = new nvchunk("memchunk1", dev, 0, 0);
    EXPECT_EQ(pc->va(), dev->va());
    EXPECT_EQ(pc->size(), dev->size());    
    
    delete pc;
}

TEST_F(nvchunkTest, NVM) {

    // file based chunk
#ifdef HAVE_LIBPMEM_H
    string path = str_pmem_mntpt + "DEV1";
#else
    string path = "/tmp/DEV1";
#endif

    unlink(path.c_str());
    NVM::instance().clear();

    EXPECT_EQ(nullptr, NVM::instance().openChunk("chunk1", path, 0, 0));

    nvchunk* pc = NVM::instance().openChunk("chunk2", path, 32, MB(13));
    ASSERT_NE(nullptr, pc);
    EXPECT_EQ(pc->size(), pc->_pDev->size() - 32);

    struct stat st;
    EXPECT_EQ(0, stat(path.c_str(), &st));
    EXPECT_EQ(st.st_size, MB(13)+32);
    EXPECT_EQ(st.st_size, pc->_pDev->size());

    char* p = (char*) pc->va();
    pc->flush();

    /* open the same chunk for the 2nd time should return existing chunk */
    nvchunk* pc1 = NVM::instance().openChunk("chunk2", path, 0, 0);
    EXPECT_EQ(pc, pc1);

    /* create a new chunk on an opened device should use existing nv_dev device */
    nvchunk* pc2 = NVM::instance().openChunk("chunk3", path, 2, MB(10));
    ASSERT_NE(pc2, nullptr);
    EXPECT_EQ(pc->_pDev, pc2->_pDev);

    /* expect nullptr if dev is null */
    EXPECT_EQ(nullptr, NVM::instance().mapChunk("chunk4", nullptr, 0, 0));

    /* mapping is shared if they have a same backing device */
    nvchunk::mapper<char> chars(pc);
    chars[0] = 'F';
    EXPECT_EQ('F', *(char*)pc->va());
    EXPECT_EQ('F', *((char*)pc2->va()+30));
    auto chars2 = pc2->getmapper<char>();
    EXPECT_EQ('F', chars2[30]);
    EXPECT_EQ('F', (char)(((char*)pc2->_pDev->va())[32]));
    chars[pc->size()-1] = 'R';
    EXPECT_EQ('R', (char)(((char*)pc->_pDev->va())[pc->_pDev->size()-1]));

    /* unmapping a chunk */
    int count = NVM::instance().nchunks();
    NVM::instance().unmapChunk("chunk2");
    EXPECT_EQ(NVM::instance().mChunks.size(), count-1);
    
    /* close a dev */
    count = NVM::instance().ndevs();
    NVM::instance().closeDev(path.c_str());
    EXPECT_EQ(NVM::instance().mDevs.size(), count-1);
    EXPECT_EQ(NVM::instance().mDevs.size(), 0);
}

int main(int argc, char **argv) {
    FLOG::set_log_devices(FLOG::LOGFILE);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}