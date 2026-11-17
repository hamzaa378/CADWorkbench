#include "core/AssemblyConstraintNetwork.h"
#include "core/AssemblyConstraintMobility.h"
#include "core/AssemblyNetworkStructureAudit.h"
#include "core/AssemblyResiliencePlanner.h"
#include "core/MetricClearanceHoleTable.h"
#include "core/CommandSearchRank.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace cad::mates;
namespace {
int checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::vector<Constraint> rigid(double dx,double dy,double dz){
    std::vector<Constraint> result;
    for(Vec3 p:{Vec3{0,0,0},Vec3{2,0,0},Vec3{0,2,0}})
        result.push_back({Kind::Point,{p.x+dx,p.y+dy,p.z+dz},p,{},{}});
    return result;
}
void testNetwork(){
    std::string issue;NetworkSolution out;
    std::vector<NetworkConnection> loop={
        {0,1,rigid(10,0,0)}, {1,2,rigid(0,20,0)},
        {2,3,rigid(-10,0,0)}, {3,0,rigid(0,-20,0)}};
    require(solveRigidNetwork(4,0,loop,&out,&issue),"closed rigid square must solve");
    require(out.worldCorrections.size()==4,"all four poses returned");
    require(std::abs(out.worldCorrections[1].translation.x-10)<1e-5,"first translation");
    require(std::abs(out.worldCorrections[2].translation.y-20)<1e-5,"second translation");
    require(std::abs(out.worldCorrections[3].translation.y-20)<1e-5,"third translation");
    require(out.worstClosureResidualMm<1e-4,"loop closure residual");
    require(out.worstPointOrAxisGapMm<1e-4,"independent linear gap audit");
    require(out.worstAngularErrorDegrees<1e-3,"independent angular audit");
    require(out.perConnection.size()==4,"every connection receives a closure report");
    for(const auto& residual:out.perConnection)
        require(residual.maximumMixedResidualMm<1e-4&&
                residual.maximumLinearGapMm<1e-4&&
                residual.maximumAngularErrorDegrees<1e-3,
                "closed loop per-connection residuals must be low");
    const auto before=out.worldCorrections;
    const auto beforeResiduals=out.perConnection;
    auto inconsistent=loop;inconsistent[3].constraints=rigid(0,-19,0);
    require(!solveRigidNetwork(4,0,inconsistent,&out,&issue),"conflicting loop must reject");
    require(issue.find("connection ")!=std::string::npos,"conflict should name the offending mate");
    require(out.worldCorrections[3].translation.y==before[3].translation.y,"output must be atomic");
    require(out.perConnection.size()==beforeResiduals.size(),
            "rejected loop must preserve the previous connection report");
    auto disconnected=loop;disconnected.pop_back();disconnected.pop_back();
    require(!solveRigidNetwork(4,0,disconnected,&out,&issue),"disconnected graph must reject");
    auto bad=loop;bad[0].parent=4;
    require(!solveRigidNetwork(4,0,bad,&out,&issue),"missing component must reject");
    bad=loop;bad[0].constraints={rigid(10,0,0)[0]};
    require(!solveRigidNetwork(4,0,bad,&out,&issue),"underconstrained edge must reject");
    require(!solveRigidNetwork(4,4,loop,&out,&issue),"invalid fixed index must reject");
    // v1.70 development audit: identify structural cycles/bridges independently
    // of the full rigid-mate geometric solve.
    cad::mates::StructuralMateAudit structural;
    const std::vector<cad::mates::StructuralMateLink> squareLinks={{0,1,3},{1,2,3},{2,3,3},{3,0,3}};
    require(cad::mates::auditMateStructure(4,0,squareLinks,&structural,&issue),"structural closed-loop audit");
    require(structural.connected&&structural.independentCycles==1&&structural.bridgeLinkIndices.empty(),
            "closed loop must have one independent graph cycle and no bridges");
    auto externalBranch=squareLinks;externalBranch.push_back({3,4,3});
    require(cad::mates::auditMateStructure(5,0,externalBranch,&structural,&issue)&&
            structural.bridgeLinkIndices.size()==1&&structural.bridgeLinkIndices.front()==4,
            "branch attachment must be identified as an exact graph bridge");
    cad::mates::StructuralReinforcement repair;
    require(cad::mates::planStructuralReinforcement(5,0,externalBranch,&repair,&issue),
            "structural mate graph reinforcement planner");
    require(repair.vulnerableFixedFrameLinksBefore==1&&
            repair.vulnerableFixedFrameLinksAfter==0&&
            repair.suggestedConnections.size()==1,
            "graph reinforcement candidate must remove single-link vulnerability");
    // Reversed traversal and a different pinned part must produce the same
    // physical relative placements, with that part fixed instead.
    require(solveRigidNetwork(4,2,loop,&out,&issue),"other fixed component");
    require(std::abs(out.worldCorrections[2].translation.y)<1e-5,"new anchor is immobile");
    // Orientation closure: four rigid 90-degree rotations around the root.
    const double pi=std::acos(-1.0);
    auto spin=[&](Vec3 p,double angle){return Vec3{
        std::cos(angle)*p.x-std::sin(angle)*p.y,
        std::sin(angle)*p.x+std::cos(angle)*p.y,p.z};};
    std::vector<NetworkConnection> rotations;
    for(std::size_t i=0;i<4;++i){
        NetworkConnection e;e.parent=i;e.child=(i+1)%4;
        for(Vec3 p:{Vec3{0,0,0},Vec3{2,0,0},Vec3{0,2,0}})
            e.constraints.push_back({Kind::Point,spin(p,pi/2),p,{},{}});
        rotations.push_back(e);
    }
    require(solveRigidNetwork(4,0,rotations,&out,&issue),"rotational closed loop must solve");
    require(out.worstClosureResidualMm<1e-4,"rotational closure is consistent");
    auto reversed=rotations;
    for(auto& e:reversed){std::swap(e.parent,e.child);for(auto& c:e.constraints)
        std::swap(c.parentPoint,c.childPoint);}
    require(solveRigidNetwork(4,0,reversed,&out,&issue),"reversed edges must solve");
    // Composition/inverse must be stable for the generated rotation matrices.
    const auto inv=inverseRigid(out.worldCorrections[1]);
    const auto identity=multiply(out.worldCorrections[1],inv);
    require(std::abs(identity.rotation[0]-1)<1e-6&&
            std::abs(identity.translation.x)<1e-6,"pose inverse round-trip");
    // Regression: near-unit direction vectors are legal inputs (1e-5 norm
    // tolerance). Exactly parallel axes must not report a false angle.
    NetworkConnection nearlyUnit{0,1,rigid(0,0,0)};
    nearlyUnit.constraints.push_back({Kind::CoaxialAxis,{},{},
                                      {0.999999,0,0},{0.999999,0,0}});
    require(solveRigidNetwork(2,0,{nearlyUnit},&out,&issue),
            "valid near-unit aligned axes must solve");
    require(out.worstAngularErrorDegrees<1e-6,
            "parallel near-unit axes must report zero angular error");
    require(out.perConnection.size()==1&&out.perConnection.front().maximumAngularErrorDegrees<1e-6,
            "per-connection near-unit axis report must also be correct");
    require(out.worstClosureResidualMm<1e-4,
            "near-unit axes remain within network closure tolerance");
    // Backend v3: a single planar mate leaves three infinitesimal DOF;
    // two nonparallel oriented frames plus one point constrain all six.
    MobilityReport mobility;
    const Vec3 origin{0,0,0},xx{1,0,0},yy{0,1,0},zz{0,0,1};
    require(analyzeConstraintMobility(2,0,{{0,1,{{Kind::CoincidentPlane,
            origin,origin,zz,zz}}}},&mobility,&issue),
            "planar mobility analysis should succeed");
    require(mobility.remainingDegreesOfFreedom==3&&
            mobility.independentConstraintRows==3,
            "plane-only mate must preserve 3 local DOF");
    require(analyzeConstraintMobility(2,0,{{0,1,{
            {Kind::Point,origin,origin,{},{}},
            {Kind::CoaxialAxis,origin,origin,xx,xx},
            {Kind::CoaxialAxis,origin,origin,yy,yy}}}},&mobility,&issue),
            "fully located frame mobility analysis");
    require(mobility.remainingDegreesOfFreedom==0&&
            mobility.independentConstraintRows==6,
            "rigid frame must have zero infinitesimal DOF");
    // Regression for production-oriented sizing and searchable part catalog.
    double diameter=0;
    require(cad::metric_holes::lookup("M6",1,&diameter)&&std::abs(diameter-6.6)<1e-12,
            "metric M6 normal clearance hole");
    require(cad::metric_holes::lookup("M12",2,&diameter)&&std::abs(diameter-14.5)<1e-12,
            "metric M12 coarse clearance hole");
    const auto beforeInvalid=diameter;
    require(!cad::metric_holes::lookup("M7",1,&diameter)&&diameter==beforeInvalid,
            "unsupported metric designation must reject atomically");
    require(cad::commandsearch::score("ISO 273 metric fastener clearance hole",
            "Create / Modeling", "bolt hole")>0,
            "search aliases find real-life metric clearance features");
    require(cad::commandsearch::score("Ball bearing","Create / Mechanical Components",
            "bushing")>0,"bearing synonym search");
    require(cad::commandsearch::score("Export DXF","File / Export","revolute")==0,
            "command search excludes irrelevant results");
}
}
int main(){try{testNetwork();std::cout<<checks<<" rigid network checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
