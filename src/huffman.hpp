#ifndef HUFFMAN_HPP_
#define HUFFMAN_HPP_

#include "bitbuffer.hpp"
#include <vector>


class HuffCode {
public:
    HuffCode();

    /** Serialize to bit buffer. */
    void toBitBuf(BitBufferWriter& buf) const;

    /** Deserialize from bit buffer. */
    void fromBitBuf(BitBufferReader& reader);


    int decodeSymbol(BitBufferReader& reader) const;

    void encodeSymbol(int data, BitBufferWriter& buf) const;

private:

};


class Huffman {
public:
    Huffman();

    void computePrefixCode(const std::vector<U64>& freqTable, HuffCode& code);

    void encode(const std::vector<int>& data, const HuffCode& code,
                BitBufferWriter& out);

    void decode(const BitBufferReader& in, const HuffCode& code,
                std::vector<int>& data);

private:

};


#endif /* HUFFMAN_HPP_ */
