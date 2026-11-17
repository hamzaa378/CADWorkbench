#include "AssemblyConstraintNetwork.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace cad::mates {
namespace {
void fail(std::string* error,const std::string& why){if(error)*error=why;}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 rotate(const std::array<double,9>& r,Vec3 p){return {
    r[0]*p.x+r[1]*p.y+r[2]*p.z,
    r[3]*p.x+r[4]*p.y+r[5]*p.z,
    r[6]*p.x+r[7]*p.y+r[8]*p.z};}
double length(Vec3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
Vec3 unitDirection(const Transform& t,Vec3 v){return rotate(t.rotation,v);}
double discrepancy(const Transform& a,const Transform& b,const Constraint& c){
    const Vec3 pa=apply(a,c.parentPoint),pb=apply(b,c.childPoint);
    if(c.kind==Kind::Point)return length(sub(pa,pb));
    const Vec3 ua=unitDirection(a,c.parentDirection),ub=unitDirection(b,c.childDirection);
    const double direction=10.0*length(sub(ua,ub));
    const Vec3 d=sub(pb,pa);
    if(c.kind==Kind::CoincidentPlane)
        return std::max(direction,std::abs(d.x*ua.x+d.y*ua.y+d.z*ua.z-c.offsetMm));
    const double axial=d.x*ua.x+d.y*ua.y+d.z*ua.z;
    return std::max(direction,length(sub(d,{axial*ua.x,axial*ua.y,axial*ua.z})));
}
} // namespace
Transform multiply(const Transform& a,const Transform& b){
    Transform out;
    for(int row=0;row<3;++row)for(int col=0;col<3;++col){
        double value=0.0;
        for(int k=0;k<3;++k)value+=a.rotation[3*row+k]*b.rotation[3*k+col];
        out.rotation[3*row+col]=value;
    }
    out.translation=add(rotate(a.rotation,b.translation),a.translation);
    out.maximumResidualMm=std::max(a.maximumResidualMm,b.maximumResidualMm);
    return out;
}
Transform inverseRigid(const Transform& t){
    Transform out;
    for(int row=0;row<3;++row)for(int col=0;col<3;++col)
        out.rotation[3*row+col]=t.rotation[3*col+row];
    const Vec3 neg{-t.translation.x,-t.translation.y,-t.translation.z};
    out.translation=rotate(out.rotation,neg);
    out.maximumResidualMm=t.maximumResidualMm;
    return out;
}
bool solveRigidNetwork(std::size_t nodeCount,std::size_t fixedNode,
                       const std::vector<NetworkConnection>& edges,
                       NetworkSolution* solution,std::string* error){
    if(!solution||nodeCount<2||nodeCount>32||fixedNode>=nodeCount||edges.empty()||edges.size()>128){
        fail(error,"Rigid network requires 2-32 components, a valid fixed component and 1-128 mates.");return false;
    }
    std::vector<Transform> relative;relative.reserve(edges.size());
    std::vector<std::vector<std::pair<std::size_t,bool>>> adjacency(nodeCount);
    for(std::size_t i=0;i<edges.size();++i){
        const auto& edge=edges[i];
        if(edge.parent>=nodeCount||edge.child>=nodeCount||edge.parent==edge.child){
            fail(error,"Assembly mate connection contains an invalid component reference.");return false;
        }
        Transform local;std::string detail;
        if(!solve(edge.constraints,&local,&detail)){
            fail(error,"Connection "+std::to_string(i+1)+" cannot establish a rigid frame: "+detail);return false;
        }
        relative.push_back(local);
        adjacency[edge.parent].emplace_back(i,true);
        adjacency[edge.child].emplace_back(i,false);
    }
    // A rooted spanning tree proposes corrections. Every non-tree edge then
    // becomes an actual closure equation checked in the original world frame.
    NetworkSolution next;
    next.worldCorrections.resize(nodeCount);
    std::vector<bool> reached(nodeCount,false);
    std::queue<std::size_t> pending;
    pending.push(fixedNode);reached[fixedNode]=true;
    while(!pending.empty()){
        const auto from=pending.front();pending.pop();
        for(const auto& entry:adjacency[from]){
            const std::size_t edgeIndex=entry.first;
            const bool parentToChild=entry.second;
            const auto& edge=edges[edgeIndex];
            const std::size_t to=parentToChild?edge.child:edge.parent;
            if(reached[to])continue;
            next.worldCorrections[to]=multiply(next.worldCorrections[from],
                                    parentToChild?relative[edgeIndex]:inverseRigid(relative[edgeIndex]));
            reached[to]=true;pending.push(to);
        }
    }
    if(std::find(reached.begin(),reached.end(),false)!=reached.end()){
        fail(error,"Assembly mate graph is disconnected from the fixed component.");return false;
    }
    double worst=0.0,worstLinear=0.0,worstAngle=0.0;
    next.perConnection.resize(edges.size());
    std::size_t worstConnection=0;
    for(std::size_t index=0;index<edges.size();++index){
        const auto& edge=edges[index];
        const auto& a=next.worldCorrections[edge.parent];
        const auto& b=next.worldCorrections[edge.child];
        auto& item=next.perConnection[index];
        for(const auto& constraint:edge.constraints){
            const double d=discrepancy(a,b,constraint);
            if(!std::isfinite(d)){fail(error,"Mate network produced a nonfinite closure error.");return false;}
            item.maximumMixedResidualMm=std::max(item.maximumMixedResidualMm,d);
            if(d>worst){worst=d;worstConnection=index+1;}
            const Vec3 pa=apply(a,constraint.parentPoint),pb=apply(b,constraint.childPoint);
            const Vec3 delta=sub(pa,pb);
            if(constraint.kind==Kind::Point){
                const double gap=length(delta);
                worstLinear=std::max(worstLinear,gap);
                item.maximumLinearGapMm=std::max(item.maximumLinearGapMm,gap);
            }
            else{
                const Vec3 ua=unitDirection(a,constraint.parentDirection);
                const Vec3 ub=unitDirection(b,constraint.childDirection);
                // Calculate the geometric angle, not acos(raw dot). The mate
                // solver deliberately accepts direction norms within 1e-5 of
                // unity. A raw dot turns identical near-unit axes into a
                // spurious nonzero angle (about 0.115 degrees at norm 0.999999).
                // atan2(|cross|, dot) also retains precision near alignment.
                const Vec3 cross{ua.y*ub.z-ua.z*ub.y,
                                 ua.z*ub.x-ua.x*ub.z,
                                 ua.x*ub.y-ua.y*ub.x};
                const double dot=ua.x*ub.x+ua.y*ub.y+ua.z*ub.z;
                const double angle=std::atan2(length(cross),dot)*180.0/3.14159265358979323846;
                worstAngle=std::max(worstAngle,angle);
                item.maximumAngularErrorDegrees=std::max(item.maximumAngularErrorDegrees,angle);
                double gap=0.0;
                if(constraint.kind==Kind::CoincidentPlane)
                    gap=std::abs(delta.x*ua.x+delta.y*ua.y+delta.z*ua.z+constraint.offsetMm);
                else{
                    const double axial=delta.x*ua.x+delta.y*ua.y+delta.z*ua.z;
                    gap=length(sub(delta,{axial*ua.x,axial*ua.y,axial*ua.z}));
                }
                worstLinear=std::max(worstLinear,gap);
                item.maximumLinearGapMm=std::max(item.maximumLinearGapMm,gap);
            }
        }
    }
    // A loop with contradictory observations cannot be solved by accepting
    // its last edge. No positions are committed on failure.
    if(worst>1e-4){
        fail(error,"Closed assembly loop is inconsistent at connection "+std::to_string(worstConnection)+
             " (gap/orientation residual exceeds the 0.0001 mm-equivalent tolerance).");
        return false;
    }
    next.worstClosureResidualMm=worst;
    next.worstPointOrAxisGapMm=worstLinear;
    next.worstAngularErrorDegrees=worstAngle;
    *solution=std::move(next);
    return true;
}
} // namespace cad::mates
