#include "../nvchunk/nvchunk.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <sstream>

using namespace std;

struct nvchunkTest : public testing::Test {
    ofstream ofs;
    ifstream ifs;

    void SetUp() { 
        std::cout << "setup" << std::endl; 
        ofs.open("/mnt/pmem/libpmem_test");
        ifs.open("/mnt/pmem/libpmem_test");
    }
 
    void TearDown() { 
        std::cout << "teardown" << std::endl;
        ofs.close();
        ifs.close();
    }
};


TEST_F(nvchunkTest, libpmem) {
    string s("try pmemlib");
    stringstream sb;
    char buf[100];

    ofs << "try pmemlib" << endl;
    ifs.getline(buf, 99);

    EXPECT_STREQ(buf, s.c_str());
}
