#ifndef WDLCOMP_HPP_
#define WDLCOMP_HPP_

#include "posindex.hpp"
#include "decisiontree.hpp"
#include <string>
#include <memory>

class BitArray;


/** Class to compactly store a WDL related information for a position. */
class WDLInfo {
public:
    int getWdl() const { return getBits(0, 3) - 2; }
    int getCaptureWdl() const { return getBits(3, 3) - 2; }
    bool getHandled() const { return getBits(7, 1); }
    U8 getData() const { return data; }

    void setWdl(int wdl) { setBits(0, 3, wdl + 2); }
    void setCaptureWdl(int wdl) { setBits(3, 3, wdl + 2); }
    void setHandled(bool handled) { setBits(7, 1, handled); }
    void setData(U8 val) { data = val; }

private:
    void setBits(int first, int size, int val) {
        int mask = ((1 << size) - 1) << first;
        data = (data & ~mask) | ((val << first) & mask);
    }

    int getBits(int first, int size) const {
        int mask = ((1 << size) - 1);
        return (data >> first) & mask;
    }

    U8 data = 0; // Bit 0-2 : wdl + 2
                 // Bit 3-5 : (best capture wdl) + 2
                 // Bit 6   : Not used
                 // Bit 7   : handled
};

/** Compress a WDL tablebase file. */
class WdlCompress {
public:
    WdlCompress(const std::string& tbType);

    void wdlDump(const std::string& outFile);

private:
    void initializeData(std::vector<WDLInfo>& data);
    void computeOptimalCaptures(std::vector<WDLInfo>& data) const;
    void computeStatistics(const std::vector<WDLInfo>& data, std::array<U64,8>& cnt) const;
    void replaceDontCares(std::vector<WDLInfo>& data, BitArray& active);
    void writeFile(const std::vector<WDLInfo>& data, const std::string& outFile) const;

    int nThreads;
    std::unique_ptr<PosIndex> posIndex;

    int bestWtm = -2;     // Score of best position for white when white's turn to move
    int bestBtm = 2;      // Score of best position for black when blacks' turn to move
};

class WDLUncompressedData : public UncompressedData {
public:
    WDLUncompressedData(std::vector<WDLInfo>& data) : data(data) {}

    int getValue(U64 idx) const override { return data[idx].getWdl(); }
    void setEncoded(U64 idx, int value) override { data[idx].setData(value); }

    bool isHandled(U64 idx) const override { return data[idx].getHandled(); }
    void setHandled(U64 idx, bool handled) override { data[idx].setHandled(!handled); }

    int getCaptureWdl(U64 idx) const { return data[idx].getCaptureWdl(); }
    void setCaptureWdl(U64 idx, int wdl) { data[idx].setCaptureWdl(wdl); }

private:
    std::vector<WDLInfo>& data;
};


#endif
