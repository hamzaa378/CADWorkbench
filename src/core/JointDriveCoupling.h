#pragma once
// Transactional algebraic coordination of articulated drive changes.
// These are a single-edit kinematic command, NOT persisted gear mates or
// full dynamic mechanism simulation. Each follower displacement is
// ratio*leader_displacement+offset in the SAME physical unit.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace cad::drive_coupling {
enum class Kind { Revolute, Prismatic };
struct Drive {
    int joint=-1;
    Kind kind=Kind::Revolute;
    double initial=0,proposed=0,minimum=0,maximum=0;
};
struct Relation {
    int leader=-1,follower=-1;
    double ratio=1,offset=0;
};
struct Solution {std::map<int,double> targets;std::vector<int> topologicalOrder;};
inline bool coordinateInRange(const Drive& d,double value){
    // Saved legacy joints with equal min/max have no authored range.
    return std::isfinite(value) && (d.minimum==d.maximum ||
        (value>=d.minimum-1e-10 && value<=d.maximum+1e-10));
}
inline bool solve(const std::vector<Drive>& drives,
                  const std::vector<Relation>& relations,Solution* out,
                  std::string* error=nullptr){
    auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!out||drives.empty()||drives.size()>64||relations.size()>63)
        return fail("Invalid coupled-drive output or drive count.");
    std::map<int,Drive> byId;
    for(const auto& d:drives){
        if(d.joint<0||!std::isfinite(d.initial)||!std::isfinite(d.proposed)||
           !std::isfinite(d.minimum)||!std::isfinite(d.maximum)||d.minimum>d.maximum||
           !byId.emplace(d.joint,d).second)
            return fail("Duplicate, invalid or nonfinite articulated drive definition.");
    }
    std::map<int,std::vector<Relation>> outbound;
    std::map<int,int> indegree;
    for(const auto& entry:byId)indegree[entry.first]=0;
    std::set<int> followers;
    for(const Relation& relation:relations){
        const auto lead=byId.find(relation.leader),follow=byId.find(relation.follower);
        if(lead==byId.end()||follow==byId.end()||lead==follow||
           !std::isfinite(relation.ratio)||!std::isfinite(relation.offset)||
           std::abs(relation.ratio)>1e6||std::abs(relation.offset)>1e6)
            return fail("Coupling must reference distinct available drives and finite bounded values.");
        if(lead->second.kind!=follow->second.kind)
            return fail("A ratio coupling cannot mix rotary angles with linear distances.");
        if(!followers.insert(relation.follower).second)
            return fail("Each follower can have only one incoming coupling.");
        outbound[relation.leader].push_back(relation);++indegree[relation.follower];
    }
    Solution staged;
    for(const auto& entry:byId)staged.targets.emplace(entry.first,entry.second.proposed);
    std::set<int> available;
    for(const auto& entry:indegree)if(entry.second==0)available.insert(entry.first);
    while(!available.empty()){
        const int id=*available.begin();available.erase(available.begin());
        const auto& d=byId.at(id);
        const double value=staged.targets.at(id);
        if(!coordinateInRange(d,value))return fail("A proposed drive lies outside its saved travel limits.");
        staged.topologicalOrder.push_back(id);
        for(const Relation& relation:outbound[id]){
            const auto& follower=byId.at(relation.follower);
            const double candidate=follower.initial+
                (value-d.initial)*relation.ratio+relation.offset;
            if(!coordinateInRange(follower,candidate))
                return fail("The coupled follower would exceed its travel limits.");
            staged.targets[relation.follower]=candidate;
            if(--indegree[relation.follower]==0)available.insert(relation.follower);
        }
    }
    if(staged.topologicalOrder.size()!=drives.size())
        return fail("Cyclic drive couplings are not accepted.");
    *out=std::move(staged);
    if(error)error->clear();return true;
}
// Analytic one-edit root travel corridors for a ratio forest. The saved
// drive bounds are applied to the entire coupled chain, rather than only
// checking the user's chosen endpoint. This is joint-coordinate feasibility,
// NOT an OCCT collision guarantee or persistent mechanical constraint.
struct RootRange {double minimum=0,maximum=0;bool lowerBounded=false,upperBounded=false;
    std::size_t constrainedFollowers=0;};
struct RangeStudy {std::map<int,RootRange> independentRoots;};
inline bool admissibleRootRanges(const std::vector<Drive>& drives,
        const std::vector<Relation>& relations,RangeStudy* output,
        std::string* error=nullptr){
    auto fail=[&](const char* detail){if(error)*error=detail;return false;};
    if(!output)return fail("A separate admissible drive-range destination is required.");
    Solution solved;
    if(!solve(drives,relations,&solved,error))return false;
    std::map<int,Drive> definitions;
    for(const auto& d:drives)definitions.emplace(d.joint,d);
    std::map<int,Relation> incoming;
    for(const auto& rel:relations)incoming.emplace(rel.follower,rel);
    struct Affine {int root=-1;double slope=0,intercept=0;};
    std::map<int,Affine> model;
    RangeStudy staged;
    for(const int joint:solved.topologicalOrder){
        const auto& d=definitions.at(joint);
        const auto relationship=incoming.find(joint);
        if(relationship==incoming.end()){
            model[joint]={joint,1,0};
            staged.independentRoots.emplace(joint,RootRange{});
        }else{
            const auto& rel=relationship->second;
            const auto& parent=model.at(rel.leader);
            const auto& leader=definitions.at(rel.leader);
            const double slope=parent.slope*rel.ratio;
            const double intercept=d.initial+(parent.intercept-leader.initial)*rel.ratio+rel.offset;
            if(!std::isfinite(slope)||!std::isfinite(intercept))
                return fail("Coupled root-range arithmetic overflowed.");
            model[joint]={parent.root,slope,intercept};
        }
        if(d.minimum==d.maximum)continue; // Authored legacy free range.
        auto& range=staged.independentRoots.at(model.at(joint).root);
        if(relationship!=incoming.end())++range.constrainedFollowers;
        const double a=model.at(joint).slope,b=model.at(joint).intercept;
        if(std::abs(a)<=1e-12){
            if(b<d.minimum-1e-10||b>d.maximum+1e-10)
                return fail("A coupled follower is outside its limits at every leader position.");
            continue;
        }
        double lo=(d.minimum-b)/a,hi=(d.maximum-b)/a;
        if(!std::isfinite(lo)||!std::isfinite(hi))
            return fail("Coupled root-range bound overflowed.");
        if(lo>hi)std::swap(lo,hi);
        if(!range.lowerBounded||lo>range.minimum){range.minimum=lo;range.lowerBounded=true;}
        if(!range.upperBounded||hi<range.maximum){range.maximum=hi;range.upperBounded=true;}
        if(range.lowerBounded&&range.upperBounded&&range.minimum>range.maximum+1e-10)
            return fail("The coupled drive chain has no feasible root interval.");
    }
    *output=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::drive_coupling
