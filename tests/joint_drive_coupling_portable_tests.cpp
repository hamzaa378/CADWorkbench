#include "core/JointDriveCoupling.h"
#include <cassert>
#include <cmath>
#include <string>
using namespace cad::drive_coupling;
static bool close(double a,double b){return std::abs(a-b)<1e-9;}
int main(){
    Solution actual;std::string issue;
    auto drives=std::vector<Drive>{{0,Kind::Revolute,0,0.5,-2,2},
                                    {1,Kind::Revolute,0.1,0.1,-2,2},
                                    {2,Kind::Revolute,-0.2,-0.2,-2,2}};
    assert(solve(drives,{{0,1,-2,0},{1,2,0.5,0}},&actual,&issue));
    assert(close(actual.targets.at(0),0.5));
    assert(close(actual.targets.at(1),-0.9));
    assert(close(actual.targets.at(2),-0.7));
    assert(actual.topologicalOrder.size()==3);
    auto before=actual;
    assert(!solve(drives,{{0,1,1,0},{1,0,1,0}},&actual,&issue));
    assert(actual.targets==before.targets);
    assert(!solve(drives,{{0,1,1,0},{2,1,1,0}},&actual,&issue));
    assert(!solve(drives,{{0,1,1e12,0}},&actual,&issue));
    assert(!solve(drives,{{0,1,5,0}},&actual,&issue));
    assert(!solve(drives,{{0,3,1,0}},&actual,&issue));
    drives[2].kind=Kind::Prismatic;
    assert(!solve(drives,{{1,2,1,0}},&actual,&issue));
    drives[2].kind=Kind::Revolute;
    drives[2].minimum=drives[2].maximum=0;
    assert(solve(drives,{{0,2,1,0}},&actual,&issue)); // legacy free limit
    // Branched and chained ratios use a topological solve rather than
    // sequential GUI writes. The whole output remains untouched on error.
    drives={{0,Kind::Revolute,0,0.5,-2,2},
            {1,Kind::Revolute,0.1,0.1,-2,2},
            {2,Kind::Revolute,0.2,0.2,-2,2},
            {3,Kind::Revolute,0.3,0.3,-2,2}};
    assert(solve(drives,{{0,1,2,0.1},{0,2,-1,0},{1,3,0.5,0.1}},
                 &actual,&issue));
    assert(close(actual.targets.at(1),1.2));
    assert(close(actual.targets.at(2),-0.3));
    assert(close(actual.targets.at(3),0.95));
    const auto snapshot=actual;
    assert(!solve(drives,{{0,1,2,0},{1,2,-1,0},{2,0,1,0}},&actual,&issue));
    assert(actual.targets==snapshot.targets);
    assert(!solve(drives,{{0,1,1,0},{2,1,1,0}},&actual,&issue));
    assert(actual.targets==snapshot.targets);
    // Full drive-graph limits can be narrower than the leader's own range.
    // Two chained followers with opposite ratios constrain the leader to
    // [-0.1,0.2] without changing any saved articulation or original result.
    drives={{0,Kind::Revolute,0,0.1,-2,2},
            {1,Kind::Revolute,0.1,0.1,-1,1},
            {2,Kind::Revolute,0.2,0.2,-0.3,0.3}};
    RangeStudy study;
    assert(admissibleRootRanges(drives,{{0,1,2,0.1},{1,2,-1,0}},&study,&issue));
    assert(study.independentRoots.size()==1);
    const auto range=study.independentRoots.at(0);
    assert(range.lowerBounded&&range.upperBounded);
    assert(close(range.minimum,-0.1)&&close(range.maximum,0.2));
    assert(range.constrainedFollowers==2);
    const auto preserved=study;
    assert(!admissibleRootRanges(drives,{{0,1,2,0.1},{1,0,-1,0}},&study,&issue));
    assert(study.independentRoots.at(0).minimum==preserved.independentRoots.at(0).minimum);
    // Legacy zero-width travel metadata means unbounded, not a locked joint.
    drives[0].minimum=drives[0].maximum=0;
    assert(admissibleRootRanges(drives,{{0,1,2,0.1},{1,2,-1,0}},&study,&issue));
    assert(close(study.independentRoots.at(0).minimum,-0.1));
    // A stationary follower whose offset is outside limits makes every
    // root position invalid, even when the leader's proposal looks legal.
    drives={{0,Kind::Prismatic,0,0,-5,5},
            {1,Kind::Prismatic,0,0,1,2}};
    assert(!admissibleRootRanges(drives,{{0,1,0,0}},&study,&issue));
    drives[0].initial=INFINITY;
    assert(!solve(drives,{},&actual,&issue));
}
