#ifndef TEST_HPP_
#define TEST_HPP_

#include <vector>


class Test {
public:
    void runTests();

private:
    // BitBuffer
    void testReadWriteBits();
    void testReadWriteU64();

    // Huffman
    void encodeDecode(const std::vector<int>& in);
    void testEncodeDecode();
    void testFibFreq();

    // PosIndex
    void testSwapColors();
};


#endif /* TEST_HPP_ */
