#ifndef HUFFMAN_HPP_
#define HUFFMAN_HPP_

#include "bitbuffer.hpp"


class HuffCode {
public:
    /** Serialize to bit buffer. */
    void toBitBuf(BitBufferWriter& buf, bool includeNumSyms) const;

    /** Deserialize from bit buffer. Get number of symbols from bit buffer. */
    void fromBitBuf(BitBufferReader& buf);
    /** Deserialize from bit buffer. Assume number of symbols is "numSyms". */
    void fromBitBuf(BitBufferReader& buf, int numSyms);

    /** Decode one symbol. */
    int decodeSymbol(BitBufferReader& buf) const;

    /** Encode one symbol. */
    void encodeSymbol(int data, BitBufferWriter& buf) const;

private:
    std::vector<int> symLen;
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
