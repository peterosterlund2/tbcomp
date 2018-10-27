#ifndef WDLCOMP_HPP_
#define WDLCOMP_HPP_

#include "posindex.hpp"
#include "decisiontree.hpp"
#include <string>
#include <memory>

class BitArray;


class WdlCompress {
public:
    WdlCompress(const std::string& tbType);

    void wdlDump(const std::string& outFile);

private:
    void initializeData(std::vector<U8>& data);
    void computeOptimalCaptures(std::vector<U8>& data) const;
    void computeStatistics(const std::vector<U8>& data, std::array<U64,8>& cnt) const;
    void replaceDontCares(std::vector<U8>& data, BitArray& active);
    void writeFile(const std::vector<U8>& data, const std::string& outFile) const;

    int nThreads;
    std::unique_ptr<PosIndex> posIndex;

    int bestWtm = -2;     // Score of best position for white when white's turn to move
    int bestBtm = 2;      // Score of best position for black when blacks' turn to move
};

class WDLUncompressedData : public UncompressedData {
public:
    WDLUncompressedData(std::vector<U8>& data) : data(data) {}

    int getValue(U64 idx) const override { return (S8)data[idx]; }
    void setValue(U64 idx, int value) override { data[idx] = (U8)value; }

    bool isActive(U64 idx) const override { return true; }
    void setActive(U64 idx, bool active) override {}

private:
    std::vector<U8>& data;
};


#endif
