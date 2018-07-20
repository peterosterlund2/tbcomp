#ifndef TEST_HPP_
#define TEST_HPP_

#include <vector>


class Test {
public:
    void runTests();

private:
    void testReadWriteBits();
    void testReadWriteU64();

    void encodeDecode(const std::vector<int>& in);
    void testEncodeDecode();
    void testFibFreq();
};


#endif /* TEST_HPP_ */
