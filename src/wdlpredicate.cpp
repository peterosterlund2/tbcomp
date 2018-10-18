#include "wdlpredicate.hpp"


std::unique_ptr<Predicate>
WDLPredicateFactory::makeMultiPredicate() {
    return make_unique<WDLMultiPredicate>();
};

double
WDLStats::entropy() const {
    return ::entropy(count.begin(), count.end());
}

std::string
WDLStats::describe() const {
    std::array<double,5> cnt;
    double tot = 0.0;
    for (int i = 0; i < 5; i++) {
        double tmp = count[i];
        cnt[i] = tmp;
        tot += tmp;
    }
    if (tot > 0) {
        for (int i = 0; i < 5; i++) {
            cnt[i] /= tot;
            cnt[i] = std::min(floor(cnt[i] * 100), 99.0);
        }
    }

    std::stringstream ss;
    ss << std::scientific << std::setprecision(2) << tot;
    ss << " [";
    for (int i = 0; i < 5; i++) {
        if (i > 0) ss << ' ';
//        ss << std::setw(2) << std::setfill('0') << (int)cnt[i];
        ss << count[i];
    }
    ss << "] " << std::setfill(' ');

    std::vector<std::pair<U64,int>> srt;
    for (int i = 0; i < 5; i++)
        srt.emplace_back(~0ULL - count[i], i);
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < 5; i++)
        ss << srt[i].second;

    ss << ' ' << entropy();

    return ss.str();
}

bool
WDLSinglePredicate::isUseful() const {
    for (int i = 0; i < 2; i++) {
        bool useful = false;
        for (int v = 0; v < 5; v++) {
            if (stats[i].count[v] != 0) {
                useful = true;
                break;
            }
        }
        if (!useful)
            return false;
    }
    return true;
}

std::string
WDLSinglePredicate::describe() const {
    WDLStats sum;
    for (int i = 0; i < 5; i++)
        sum.count[i] = stats[0].count[i] + stats[1].count[i];

    std::stringstream ss;
    ss << sum.describe() << ' ' << describePredName();
    ss << ' ' << std::scientific << std::setprecision(2) << entropy();

    return ss.str();
}

std::string
WDLSinglePredicate::describeChild(bool predVal) const {
    int idx = predVal;
    return stats[idx].describe();
}

Predicate::ApplyResult
WDLMultiPredicate::applyData(const Position& pos, int value) {
    wtmPred.updateStats(pos, value);
    inCheckPred.updateStats(pos, value);
    bbpPredW.updateStats(pos, value);
    bbpPredB.updateStats(pos, value);
    return PRED_NONE;
}

std::unique_ptr<Predicate>
WDLMultiPredicate::getBest() const {
    std::unique_ptr<SinglePredicate> best;
    best = wtmPred.getBest(std::move(best));
    best = inCheckPred.getBest(std::move(best));
    best = bbpPredW.getBest(std::move(best));
    best = bbpPredB.getBest(std::move(best));
    return best;
}
