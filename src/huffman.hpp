#ifndef HUFFMAN_HPP_
#define HUFFMAN_HPP_

#include "bitbuffer.hpp"


/** Symbol encoding/decoding using Huffman prefix codes. */
class HuffCode {
public:
    /** Compute canoncial Huffman code from symbol lengths. */
    void setSymbolLengths(const std::vector<int>& bitLenVec);

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
    /** Compute canonical Huffman tree from symbol lengths. */
    void computeTree();

    std::vector<int> symLen;  // Symbol lengths
    std::vector<U64> symBits; // Symbol bit patterns

    struct Node {
        int left;   // Left child, or -symVal if left child is a leaf node
        int right;  // Right child, or -symVal if right child is a leaf node
    };
    std::vector<Node> nodes;
};


/** Utility for creating a Huffman code and for encoding/decoding
 *  an array of symbols. */
class Huffman {
public:
    void computePrefixCode(const std::vector<U64>& freqTable, HuffCode& code);

    void encode(const std::vector<int>& data, const HuffCode& code,
                BitBufferWriter& out);

    void decode(BitBufferReader& in, U64 nSymbols, const HuffCode& code,
                std::vector<int>& data);
};


#endif /* HUFFMAN_HPP_ */
