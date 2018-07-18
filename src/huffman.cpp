#include "huffman.hpp"
#include <queue>
#include <functional>
#include <cassert>

#include <iostream>


int
HuffCode::decodeSymbol(BitBufferReader& reader) const {
    return 0;
}

void
HuffCode::encodeSymbol(int data, BitBufferWriter& buf) const {
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

    std::cout << "lenVec: " << lenVec << std::endl;
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
