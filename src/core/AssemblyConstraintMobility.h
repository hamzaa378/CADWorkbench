#pragma once
#include "AssemblyConstraintNetwork.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// Analyze instantaneous degrees of freedom of an ALREADY ALIGNED rigid-mate
// graph, without trying to force an underconstrained network into the fully
// constrained rigid-network solver. This is local Jacobian rank, NOT a proof
// of finite mechanism motion, kinematic closure, or collision freedom.
// Units are millimetres and radians; normals must be unit world vectors.
// A solved configuration is required because an unsatisfied mate has no
// meaningful Jacobian at the requested nominal assembly configuration.
namespace cad::mates {
struct MobilityReport {
    int kinematicVariables=0;
    int independentConstraintRows=0;
    int redundantConstraintRows=0;
    int remainingDegreesOfFreedom=0;
    std::vector<std::size_t> disconnectedOccurrences;
    std::vector<std::size_t> whollyRedundantConnections; // 1-based
    std::vector<std::size_t> potentiallyMobileOccurrences;
};
namespace mobility_detail {
inline Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 mul(Vec3 a,double f){return {a.x*f,a.y*f,a.z*f};}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(Vec3 a){return std::hypot(std::hypot(a.x,a.y),a.z);}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline Vec3 tangent(Vec3 n){
    const Vec3 axis=std::abs(n.z)<0.9?Vec3{0,0,1}:Vec3{0,1,0};
    const Vec3 c=cross(axis,n);
    return mul(c,1.0/length(c));
}
} // namespace mobility_detail
inline bool analyzeConstraintMobility(std::size_t nodeCount,std::size_t fixedNode,
     const std::vector<NetworkConnection>& connections,MobilityReport* output,
     std::string* error=nullptr){
    using namespace mobility_detail;
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!output||nodeCount<2||nodeCount>32||fixedNode>=nodeCount||connections.size()>128)
        return fail("Mobility analysis needs 2-32 nodes, a valid fixed node, and up to 128 connections.");
    std::size_t count=0;
    std::vector<Vec3> origins(nodeCount);
    std::vector<int> observations(nodeCount,0);
    std::vector<std::vector<std::size_t>> adjacency(nodeCount);
    for(const auto& edge:connections){
        if(edge.parent>=nodeCount||edge.child>=nodeCount||edge.parent==edge.child||
           edge.constraints.empty()||edge.constraints.size()>16)
            return fail("Mate connections must join distinct valid nodes with 1-16 observations.");
        if((count+=edge.constraints.size())>512)
            return fail("Mobility analysis supports at most 512 geometric observations.");
        adjacency[edge.parent].push_back(edge.child);
        adjacency[edge.child].push_back(edge.parent);
        for(const auto& c:edge.constraints){
            if(!finite(c.parentPoint)||!finite(c.childPoint)||!finite(c.parentDirection)||
               !finite(c.childDirection)||!std::isfinite(c.offsetMm))
                return fail("Mate observation contains nonfinite geometry.");
            if(c.kind!=Kind::Point&&c.kind!=Kind::CoaxialAxis&&
               c.kind!=Kind::CoincidentPlane)
                return fail("Unsupported mate observation kind.");
            if(c.kind==Kind::Point){
                if(c.offsetMm!=0.0||length(sub(c.parentPoint,c.childPoint))>1e-4)
                    return fail("Point mates must be coincident before mobility analysis.");
            }else{
                if(std::abs(length(c.parentDirection)-1.0)>1e-5||
                   std::abs(length(c.childDirection)-1.0)>1e-5||
                   length(sub(c.parentDirection,c.childDirection))>1e-4)
                    return fail("Mate axes must be aligned, unit world directions.");
                const Vec3 displacement=sub(c.childPoint,c.parentPoint);
                const double signedOffset=dot(displacement,c.parentDirection);
                if(c.kind==Kind::CoaxialAxis){
                    if(c.offsetMm!=0.0||
                       length(sub(displacement,mul(c.parentDirection,signedOffset)))>1e-4)
                        return fail("Coaxial mate lines are not coincident.");
                }else if(std::abs(signedOffset-c.offsetMm)>1e-4){
                    return fail("Plane mate separation does not match its signed offset.");
                }
            }
            origins[edge.parent]=add(origins[edge.parent],c.parentPoint);
            origins[edge.child]=add(origins[edge.child],c.childPoint);
            ++observations[edge.parent];++observations[edge.child];
        }
    }
    for(std::size_t i=0;i<nodeCount;++i)
        if(observations[i])origins[i]=mul(origins[i],1.0/double(observations[i]));
    // Local translational variables are velocities at the mean observation
    // point. Using local origins avoids huge rotational Jacobian coefficients
    // for CAD models far from world zero.
    const std::size_t columns=(nodeCount-1)*6;
    const auto variableIndex=[&](std::size_t node){
        return 6*(node<fixedNode?node:node-1);
    };
    std::vector<std::vector<double>> basis;
    basis.reserve(columns);
    int equationCount=0;
    int rank=0;
    std::vector<std::size_t> fullyRedundant;
    const auto makeRow=[&](){return std::vector<double>(columns,0.0);};
    const auto velocity=[&](std::vector<double>& row,std::size_t node,
                            Vec3 at,Vec3 projection,double sign){
        if(node==fixedNode)return;
        const std::size_t base=variableIndex(node);
        row[base]+=sign*projection.x;
        row[base+1]+=sign*projection.y;
        row[base+2]+=sign*projection.z;
        const Vec3 torque=cross(sub(at,origins[node]),projection);
        row[base+3]+=sign*torque.x;
        row[base+4]+=sign*torque.y;
        row[base+5]+=sign*torque.z;
    };
    const auto angular=[&](std::vector<double>& row,std::size_t node,
                           Vec3 tangentDirection,double sign){
        if(node==fixedNode)return;
        const std::size_t base=variableIndex(node)+3;
        row[base]+=sign*tangentDirection.x;
        row[base+1]+=sign*tangentDirection.y;
        row[base+2]+=sign*tangentDirection.z;
    };
    const auto insertRankRow=[&](std::vector<double> row)->bool{
        ++equationCount;
        double norm=0.0;
        for(const double value:row)norm=std::hypot(norm,value);
        if(!std::isfinite(norm))return false;
        if(norm<1e-12)return true;
        for(double& value:row)value/=norm;
        for(int pass=0;pass<2;++pass){
            for(const auto& b:basis){
                double projection=0.0;
                for(std::size_t j=0;j<columns;++j)projection+=row[j]*b[j];
                for(std::size_t j=0;j<columns;++j)row[j]-=projection*b[j];
            }
        }
        double remainder=0.0;
        for(const double value:row)remainder=std::hypot(remainder,value);
        if(!std::isfinite(remainder))return false;
        if(remainder>1e-8){
            for(double& value:row)value/=remainder;
            basis.push_back(std::move(row));++rank;
        }
        return true;
    };
    for(std::size_t ei=0;ei<connections.size();++ei){
        const auto& edge=connections[ei];
        const int previousRank=rank;
        for(const auto& c:edge.constraints){
            const Vec3 pa=c.parentPoint,pb=c.childPoint;
            const auto addTranslation=[&](Vec3 direction)->bool{
                auto row=makeRow();
                velocity(row,edge.parent,pa,direction,-1);
                velocity(row,edge.child,pb,direction,+1);
                return insertRankRow(std::move(row));
            };
            if(c.kind==Kind::Point){
                if(!addTranslation({1,0,0})||!addTranslation({0,1,0})||
                   !addTranslation({0,0,1}))return fail("Mobility rank overflow.");
                continue;
            }
            const Vec3 n=c.parentDirection;
            const Vec3 t1=tangent(n),t2=cross(n,t1);
            for(const Vec3 t:{t1,t2}){
                auto row=makeRow();
                angular(row,edge.parent,t,-1);
                angular(row,edge.child,t,+1);
                if(!insertRankRow(std::move(row)))return fail("Mobility rank overflow.");
            }
            if(c.kind==Kind::CoaxialAxis){
                if(!addTranslation(t1)||!addTranslation(t2))
                    return fail("Mobility rank overflow.");
            }else if(!addTranslation(n)){
                return fail("Mobility rank overflow.");
            }
        }
        if(rank==previousRank)fullyRedundant.push_back(ei+1);
    }
    std::vector<bool> reached(nodeCount,false);
    std::queue<std::size_t> pending;
    pending.push(fixedNode);reached[fixedNode]=true;
    while(!pending.empty()){
        const auto from=pending.front();pending.pop();
        for(auto child:adjacency[from])if(!reached[child]){
            reached[child]=true;pending.push(child);
        }
    }
    MobilityReport next;
    next.kinematicVariables=static_cast<int>(columns);
    next.independentConstraintRows=rank;
    next.redundantConstraintRows=equationCount-rank;
    next.remainingDegreesOfFreedom=static_cast<int>(columns)-rank;
    next.whollyRedundantConnections=std::move(fullyRedundant);
    for(std::size_t i=0;i<nodeCount;++i){
        if(!reached[i])next.disconnectedOccurrences.push_back(i);
        if(i==fixedNode)continue;
        // Projection onto the nullspace identifies whether at least one
        // translational or rotational coordinate participates in a local
        // unconstrained motion. Components of a coupled motion are NOT
        // independently actuated degrees of freedom.
        const std::size_t base=variableIndex(i);
        bool mobile=false;
        for(std::size_t column=base;column<base+6;++column){
            double projected=0.0;
            for(const auto& b:basis)projected+=b[column]*b[column];
            if(projected<1.0-1e-7){mobile=true;break;}
        }
        if(mobile)next.potentiallyMobileOccurrences.push_back(i);
    }
    *output=std::move(next);
    if(error)error->clear();
    return true;
}
} // namespace cad::mates
