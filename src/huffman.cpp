#include "huffman.hpp"
#include <queue>
#include <functional>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
#include <sstream>
#include <iomanip>


void HuffCode::setSymbolLengths(const std::vector<int>& bitLenVec) {
    symLen = bitLenVec;
    for (int s : symLen)
        assert(s < 64);
    computeTree();
}

void HuffCode::toBitBuf(BitBufferWriter& buf, bool includeNumSyms) const {
    if (includeNumSyms)
        buf.writeU64(symLen.size());

    int maxVal = *std::max_element(symLen.begin(), symLen.end());
    int symBits = 0;
    while (maxVal > 0) {
        maxVal /= 2;
        symBits++;
    }
    buf.writeU64(symBits);
    for (int s : symLen)
        buf.writeBits(s, symBits);
}

void HuffCode::fromBitBuf(BitBufferReader& buf) {
    int numSyms = buf.readU64();
    fromBitBuf(buf, numSyms);
}

void HuffCode::fromBitBuf(BitBufferReader& buf, int numSyms) {
    int symBits = buf.readU64();
    symLen.resize(numSyms);
    for (int i = 0; i < numSyms; i++)
        symLen[i] = buf.readBits(symBits);
    computeTree();
}

static std::string
printBits(U64 val, int nBits) {
    std::stringstream ss;
    for (int i = nBits -1; i >= 0; i--) {
        bool b = ((1ULL << i) & val) != 0;
        ss << (b ? '1' : '0');
    }
    return ss.str();
}

void HuffCode::computeTree() {
    const int nSym = symLen.size();
    std::vector<std::pair<int,int>> syms;
    for (int i = 0; i < nSym; i++)
        syms.push_back(std::make_pair(symLen[i], i));
    std::sort(syms.begin(), syms.end());
    symBits.resize(nSym);
    U64 bits = 0;
    for (int i = 0; i < nSym; i++) {
        symBits[syms[i].second] = bits;
        if (i < nSym - 1)
            bits = (bits + 1) << (syms[i+1].first - syms[i].first);
    }

    for (int i = 0; i < nSym; i++) {
        int len = syms[i].first;
        int sym = syms[i].second;
        if (len > 0) {
            std::cout << "len:" << std::setw(2) << len
                      << " sym:" << std::setw(3) << sym
                      << " bits: " << printBits(symBits[sym], len) << std::endl;
        }
    }
}

int
HuffCode::decodeSymbol(BitBufferReader& buf) const {
    return 0;
}

void
HuffCode::encodeSymbol(int data, BitBufferWriter& buf) const {
    buf.writeBits(symBits[data], symLen[data]);
}


#if 1
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    os << "[";
    bool first = true;
    for (const T& e : v) {
        if (!first)
            os << ", ";
        os << e;
        first = false;
    }
    os << " ]";
    return os;
}
#endif

void Huffman::computePrefixCode(const std::vector<U64>& freqTable, HuffCode& code) {
    const int nSym = freqTable.size();
    assert(nSym > 0);

    struct Node {
        U64 freq;    // Total frequency for subtree rooted at this node
        int id;      // Node identifier
        int child1;  // Left child id or -1 for a leaf node
        int child2;  // Right child id or -1 for a leaf node

        bool operator<(const Node& n) const {
            return n.freq < freq;
        }
    };

    std::vector<Node> nodes;
    std::priority_queue<Node> tree;

    for (int i = 0; i < nSym; i++) {
        Node n { freqTable[i], i, -1, -1 };
        nodes.push_back(n);
        tree.push(n);
    }

    while (tree.size() > 1) {
        Node n1 = tree.top(); tree.pop();
        if (n1.freq == 0)
            continue;
        Node n2 = tree.top(); tree.pop();
        Node n { n1.freq + n2.freq, (int)nodes.size(), n1.id, n2.id };
        nodes.push_back(n);
        tree.push(n);
    }

    std::vector<int> lenVec(nSym);

    std::function<void(int,int)> computeLengths =
            [&computeLengths,&nodes,&lenVec](int id, int depth) {
        const Node& n = nodes[id];
        if (n.child1 == -1) {
            lenVec[n.id] = depth;
        } else {
            computeLengths(n.child1, depth + 1);
            computeLengths(n.child2, depth + 1);
        }
    };
    computeLengths(tree.top().id, 0);

    std::cout << "freq: " << freqTable << std::endl;
    std::vector<U64> freqSorted(freqTable);
    std::sort(freqSorted.begin(), freqSorted.end(), std::greater<U64>());
    std::cout << "freqS: " << freqSorted << std::endl;

    std::cout << "lenVec: " << lenVec << std::endl;
    std::vector<int> sorted(lenVec);
    std::sort(sorted.begin(), sorted.end());
    std::cout << "lenVecS: " << sorted << std::endl;

    U64 totFreq = 0;
    double entr = 0.0;
    for (U64 f : freqTable) {
        if (f == 0)
            continue;
        entr += -(f * std::log2((double)f));
        totFreq += f;
    }
    entr += totFreq * std::log2((double)totFreq);
    entr /= 8;

    U64 compr = 0;
    for (int i = 0; i < nSym; i++)
        compr += freqTable[i] * lenVec[i];
    compr /= 8;

    std::cout << "size: " << totFreq
              << " entr: " << entr << ' ' << (entr / totFreq)
              << " compr: " << compr << ' ' << (compr / (double)totFreq)
              << std::endl;

    code.setSymbolLengths(lenVec);
}

void Huffman::encode(const std::vector<int>& data, const HuffCode& code,
                     BitBufferWriter& out) {
    for (int d : data)
        code.encodeSymbol(d, out);
}

void Huffman::decode(BitBufferReader& in, U64 nSymbols, const HuffCode& code,
                     std::vector<int>& data) {
    data.reserve(nSymbols);
    while (nSymbols-- != 0)
        data.push_back(code.decodeSymbol(in));
}
