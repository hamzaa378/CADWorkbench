#pragma once

#include <algorithm>
#include <cmath>

// Bounded, conservative geometric matching for a *full* circular edge after a
// radius-only model edit. This is intentionally not a universal topology-name
// resolver. Caller must inspect EVERY candidate, reject multiple matches and
// verify that the edge is a complete circle in the current B-rep.
namespace cad::circular_lineage {
struct Point3 {double x=0,y=0,z=0;};
struct Reference {Point3 center,axis;double radius=0;};
inline double lengthSquared(Point3 p) noexcept {return p.x*p.x+p.y*p.y+p.z*p.z;}
inline bool finite(Point3 p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
inline bool compatible(Reference saved,Reference candidate,double maxRelativeRadiusChange=0.20) noexcept {
    if(!finite(saved.center)||!finite(saved.axis)||!finite(candidate.center)||
       !finite(candidate.axis)||!std::isfinite(saved.radius)||!std::isfinite(candidate.radius)||
       !std::isfinite(maxRelativeRadiusChange)||saved.radius<=1e-8||candidate.radius<=1e-8||
       maxRelativeRadiusChange<0||maxRelativeRadiusChange>0.20)return false;
    const double oldNorm=lengthSquared(saved.axis),newNorm=lengthSquared(candidate.axis);
    if(std::abs(oldNorm-1.0)>0.01||std::abs(newNorm-1.0)>0.01)return false;
    const double alignment=saved.axis.x*candidate.axis.x+
        saved.axis.y*candidate.axis.y+saved.axis.z*candidate.axis.z;
    if(std::abs(alignment)<0.99999)return false;
    const double centerTolerance=std::max(1e-4,saved.radius*1e-5);
    const Point3 delta{candidate.center.x-saved.center.x,
                       candidate.center.y-saved.center.y,
                       candidate.center.z-saved.center.z};
    if(lengthSquared(delta)>centerTolerance*centerTolerance)return false;
    return std::abs(candidate.radius-saved.radius)<=
        maxRelativeRadiusChange*saved.radius+1e-9;
}
} // namespace cad::circular_lineage
