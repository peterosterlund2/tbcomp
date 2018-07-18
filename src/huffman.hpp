#ifndef HUFFMAN_HPP_
#define HUFFMAN_HPP_

#include "bitbuffer.hpp"


class HuffCode {
public:
    /** Serialize to bit buffer. */
    void toBitBuf(BitBufferWriter& buf) const;

    /** Deserialize from bit buffer. */
    void fromBitBuf(BitBufferReader& reader);

    /** Decode one symbol. */
    int decodeSymbol(BitBufferReader& reader) const;

    /** Encode one symbol. */
    void encodeSymbol(int data, BitBufferWriter& buf) const;

private:

};


class Huffman {
public:
    void computePrefixCode(const std::vector<U64>& freqTable, HuffCode& code);

    void encode(const std::vector<int>& data, const HuffCode& code,
                BitBufferWriter& out);

    void decode(BitBufferReader& in, U64 nSymbols, const HuffCode& code,
                std::vector<int>& data);
};


#endif /* HUFFMAN_HPP_ */
