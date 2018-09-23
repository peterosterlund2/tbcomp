#ifndef WDLCOMP_HPP_
#define WDLCOMP_HPP_

#include "posindex.hpp"

#include <string>
#include <memory>


class WdlCompress {
public:
    WdlCompress(const std::string& tbType);

    void wdlDump(const std::string& outFile);

private:
    void initializeData(std::vector<U8>& data);
    void computeOptimalCaptures(std::vector<U8>& data) const;
    int computeStatistics(const std::vector<U8>& data, std::array<U64,8>& cnt) const;
    void replaceDontCares(std::vector<U8>& data, int mostFreq);
    void writeFile(const std::vector<U8>& data, const std::string& outFile) const;

    int nThreads;
    std::unique_ptr<PosIndex> posIndex;

    int bestWtm = -2;     // Score of best position for white when white's turn to move
    int bestBtm = 2;      // Score of best position for black when blacks' turn to move
};

#endif
