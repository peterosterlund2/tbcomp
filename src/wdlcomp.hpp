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
    static void wdlDump();

    std::unique_ptr<PosIndex> posIndex;
};

#endif
