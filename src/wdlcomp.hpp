#ifndef WDLCOMP_HPP_
#define WDLCOMP_HPP_

#include "posindex.hpp"
#include "decisiontree.hpp"
#include "wdlnode.hpp"
#include <string>
#include <memory>

class BitArray;


/** Compress a WDL tablebase file. */
class WdlCompress {
public:
    WdlCompress(const std::string& tbType, bool useGini, double mergeThreshold,
                int samplingLogFactor);

    void wdlDump(const std::string& outFile, int maxTreeDepth, int maxCollectorNodes);

private:
    void initializeData(std::vector<WDLInfo>& data);
    /** Return WDL score (white perspective) for best capture. */
    static int wdlBestCapture(Position& pos);
    void computeOptimalCaptures(std::vector<WDLInfo>& data) const;
    void computeStatistics(const std::vector<WDLInfo>& data, std::array<U64,8>& cnt) const;
    void replaceDontCares(std::vector<WDLInfo>& data, BitArray& active);
    void writeFile(const std::vector<WDLInfo>& data, const std::string& outFile) const;

    const bool useGini;
    const double mergeThreshold;
    const int samplingLogFactor;
    int nThreads;
    std::unique_ptr<PosIndex> posIndex;

    int bestWtm = -2;     // Score of best position for white when white's turn to move
    int bestBtm = 2;      // Score of best position for black when blacks' turn to move
};


#endif
