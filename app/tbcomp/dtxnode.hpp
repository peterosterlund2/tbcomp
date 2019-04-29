#ifndef DTXNODE_HPP_
#define DTXNODE_HPP_

#include "dtnode.hpp"


#if 0

/** Statistics node for DTZ/DTM metrics. */
class DTXStatsNode : public StatsNode {
    DTXStats stats;
};

class DTXStatsCollectorNode : public StatsCollectorNode {
    StatsCollector<WTMPredicate, DTXStats> wtm;
    StatsCollector<InCheckPredicate, DTXStats> inCheck;
    StatsCollector<BishopPairPredicate<true>, DTXStats> bPairW;
    StatsCollector<BishopPairPredicate<false>, DTXStats> bPairB;
};

#endif



#endif /* DTXNODE_HPP_ */
