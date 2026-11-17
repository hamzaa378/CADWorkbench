#include "AdaptiveMotionScan.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <numeric>
#include <utility>

namespace cad::motion {
namespace {
void fail(std::string* error,const char* reason){if(error)*error=reason;}
bool boundEnabled(const Settings& settings){
    return settings.provenRelativeSpeedBoundMmPerSecond>=0.0;
}
// The distance between moving solids is Lipschitz continuous when an upper
// bound on relative surface speed is known. For any time in [a,b], an endpoint
// lies at most (b-a)/2 away. A deliberately conservative bound is therefore
// min(clearance(a),clearance(b)) - speedBound*(b-a)/2.
double guaranteedClearance(const Observation& a,const Observation& b,const Settings& settings){
    return std::min(a.clearanceMm,b.clearanceMm)-
           settings.provenRelativeSpeedBoundMmPerSecond*(b.timeSeconds-a.timeSeconds)*0.5;
}
bool speedBoundConsistent(const Observation& a,const Observation& b,const Settings& settings){
    if(!boundEnabled(settings))return true;
    const double upper=settings.provenRelativeSpeedBoundMmPerSecond*(b.timeSeconds-a.timeSeconds);
    return std::abs(a.clearanceMm-b.clearanceMm)<=upper+1e-7;
}
bool provedClearance(const Observation& a,const Observation& b,const Settings& settings){
    return boundEnabled(settings)&&a.overlapVolumeMm3==0.0&&b.overlapVolumeMm3==0.0&&
           guaranteedClearance(a,b,settings)>settings.safetyClearanceMm+1e-8;
}
struct Interval {
    double first=0,last=0;
    bool risk=false;
    double width=0;
};
bool needsRefinement(const Observation& a,const Observation& b,const Settings& settings,
                     Interval* out){
    // An interval provably beyond the requested safety margin never needs
    // extra samples. This short circuit is available ONLY with a valid bound.
    if(provedClearance(a,b,settings))return false;
    const double width=b.timeSeconds-a.timeSeconds;
    if(!(width>settings.fineStepSeconds)||!(std::midpoint(a.timeSeconds,b.timeSeconds)>a.timeSeconds) ||
       !(std::midpoint(a.timeSeconds,b.timeSeconds)<b.timeSeconds))return false;
    const bool risk=a.overlapVolumeMm3>0||b.overlapVolumeMm3>0||
        std::min(a.clearanceMm,b.clearanceMm)<=settings.safetyClearanceMm||
        std::abs(a.clearanceMm-b.clearanceMm)>settings.safetyClearanceMm;
    if(width<= (risk?settings.fineStepSeconds:settings.coarseStepSeconds))return false;
    if(out)*out={a.timeSeconds,b.timeSeconds,risk,width};
    return true;
}
} // namespace
bool scanAdaptive(const std::vector<double>& initialTimes,const Settings& settings,
                  const Evaluate& evaluate,Result* result,std::string* error){
    if(!result||!evaluate||initialTimes.size()<2||initialTimes.size()>256||
       settings.maximumSamples<2||settings.maximumSamples>256||
       initialTimes.size()>static_cast<std::size_t>(settings.maximumSamples)||
       !std::isfinite(settings.safetyClearanceMm)||settings.safetyClearanceMm<0||
       !std::isfinite(settings.coarseStepSeconds)||settings.coarseStepSeconds<=0||
       !std::isfinite(settings.fineStepSeconds)||settings.fineStepSeconds<=0||
       settings.fineStepSeconds>settings.coarseStepSeconds||
       !std::isfinite(settings.provenRelativeSpeedBoundMmPerSecond)||
       (settings.provenRelativeSpeedBoundMmPerSecond<0&&
        settings.provenRelativeSpeedBoundMmPerSecond!=-1.0)){

        fail(error,"Invalid adaptive motion settings, sample budget, or output.");return false;
    }
    for(std::size_t i=0;i<initialTimes.size();++i){
        if(!std::isfinite(initialTimes[i])||initialTimes[i]<0||initialTimes[i]>3600||
           (i>0&&initialTimes[i]<=initialTimes[i-1])){
            fail(error,"Motion timestamps must be finite, strictly increasing and within 0-3600 seconds.");
            return false;
        }
    }
    std::map<double,Observation> measured;
    for(const double time:initialTimes){
        Observation item;
        std::string reason;
        if(!evaluate(time,&item,&reason)){
            if(error)*error=reason.empty()?"Motion evaluation failed.":reason;
            return false;
        }
        if(!std::isfinite(item.clearanceMm)||item.clearanceMm<0||
           !std::isfinite(item.overlapVolumeMm3)||item.overlapVolumeMm3<0){
            fail(error,"Motion evaluation returned invalid physical measurements.");return false;
        }
        item.timeSeconds=time; // Do not trust callback-supplied time.
        measured.emplace(time,item);
    }
    // When a user-supplied speed bound disagrees with measured clearance,
    // fail closed. Do not emit a misleading conditional safety claim.
    const auto checkBound=[&]()->bool{
        if(!boundEnabled(settings))return true;
        for(auto it=measured.begin();it!=measured.end();++it){
            const auto next=std::next(it);
            if(next!=measured.end()&&!speedBoundConsistent(it->second,next->second,settings)){
                fail(error,"The supplied relative speed bound contradicts measured clearance changes.");
                return false;
            }
        }
        return true;
    };
    if(!checkBound())return false;
    // Prioritize risk, then the widest unresolved time interval. Ties are
    // broken by earlier time to make test runs and exported results repeatable.
    while(measured.size()<static_cast<std::size_t>(settings.maximumSamples)){
        Interval best;
        bool has=false;
        for(auto it=measured.begin();it!=measured.end();++it){
            const auto next=std::next(it);
            if(next==measured.end())break;
            Interval interval;
            if(!needsRefinement(it->second,next->second,settings,&interval))continue;
            if(!has||(interval.risk&&!best.risk)||
               (interval.risk==best.risk&&interval.width>best.width)||
               (interval.risk==best.risk&&interval.width==best.width&&interval.first<best.first)){
                best=interval;has=true;
            }
        }
        if(!has)break;
        const double midpoint=std::midpoint(best.first,best.last);
        Observation item;
        std::string reason;
        if(!evaluate(midpoint,&item,&reason)){
            if(error)*error=reason.empty()?"Refined motion evaluation failed.":reason;
            return false;
        }
        if(!std::isfinite(item.clearanceMm)||item.clearanceMm<0||
           !std::isfinite(item.overlapVolumeMm3)||item.overlapVolumeMm3<0){
            fail(error,"Refined motion evaluation returned invalid physical measurements.");
            return false;
        }
        item.timeSeconds=midpoint;
        measured.emplace(midpoint,item);
        if(!checkBound())return false;
    }
    Result candidate;
    candidate.observations.reserve(measured.size());
    for(const auto& [time,observation]:measured)candidate.observations.push_back(observation);
    for(auto it=measured.begin();it!=measured.end();++it){
        const auto next=std::next(it);
        if(next!=measured.end()&&needsRefinement(it->second,next->second,settings,nullptr))
            ++candidate.intervalsNeedingRefinement;
    }
    candidate.budgetExhausted=candidate.intervalsNeedingRefinement>0;
    double least=std::numeric_limits<double>::infinity();
    bool encounteredOverlap=false;
    for(const auto& [time,observation]:measured)
        encounteredOverlap=encounteredOverlap||observation.overlapVolumeMm3>0.0;
    candidate.intervals.reserve(measured.size()-1);
    for(auto it=measured.begin();it!=measured.end();++it){
        const auto next=std::next(it);
        if(next==measured.end())break;
        const bool bounded=boundEnabled(settings);
        const bool proved=provedClearance(it->second,next->second,settings);
        const double lower=bounded?guaranteedClearance(it->second,next->second,settings):0.0;
        candidate.intervals.push_back({it->first,next->first,
            std::min(it->second.clearanceMm,next->second.clearanceMm),
            lower,bounded,
            it->second.overlapVolumeMm3>0.0||next->second.overlapVolumeMm3>0.0,
            proved,needsRefinement(it->second,next->second,settings,nullptr)});
        if(!proved)++candidate.intervalsWithoutClearanceProof;
        if(bounded)least=std::min(least,lower);
    }
    candidate.clearanceBoundSatisfied=boundEnabled(settings)&&!encounteredOverlap&&
        candidate.intervalsWithoutClearanceProof==0;
    if(boundEnabled(settings))candidate.conservativeClearanceLowerBoundMm=std::max(0.0,least);
    *result=std::move(candidate);
    if(error)error->clear();
    return true;
}
} // namespace cad::motion
