#pragma once
// Direction-independent finite cutter extent for arbitrary solid B-reps.
// This is a pure numeric helper; it does not claim a valid Boolean result.
#include <algorithm>
#include <cmath>

namespace cad::directional_holes {
struct Vec3 { double x=0.0,y=0.0,z=0.0; };
struct Bounds { double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0; };
struct Span { double start=0.0,length=0.0; };

inline bool throughAllSpan(const Bounds& box,const Vec3& entry,const Vec3& direction,
                           Span* output) {
    if(!output) return false;
    const double values[]={box.xmin,box.ymin,box.zmin,box.xmax,box.ymax,box.zmax,
                           entry.x,entry.y,entry.z,direction.x,direction.y,direction.z};
    for(double v:values) if(!std::isfinite(v)||std::abs(v)>1e9) return false;
    if(box.xmin>box.xmax||box.ymin>box.ymax||box.zmin>box.zmax) return false;
    const double magnitude=std::hypot(direction.x,direction.y,direction.z);
    if(!std::isfinite(magnitude)||magnitude<1e-12) return false;
    const double dx=direction.x/magnitude,dy=direction.y/magnitude,dz=direction.z/magnitude;
    double minProjection=0.0,maxProjection=0.0;
    for(int ix=0;ix<2;++ix)for(int iy=0;iy<2;++iy)for(int iz=0;iz<2;++iz){
        const double x=(ix?box.xmax:box.xmin)-entry.x;
        const double y=(iy?box.ymax:box.ymin)-entry.y;
        const double z=(iz?box.zmax:box.zmin)-entry.z;
        const double t=x*dx+y*dy+z*dz;
        minProjection=std::min(minProjection,t);
        maxProjection=std::max(maxProjection,t);
    }
    const double padding=std::max(0.01,(maxProjection-minProjection)*1e-4);
    const Span candidate{minProjection-padding,maxProjection-minProjection+2*padding};
    if(!std::isfinite(candidate.start)||!std::isfinite(candidate.length)||
       candidate.length<=0.0||candidate.length>1e9)return false;
    *output=candidate;
    return true;
}
} // namespace cad::directional_holes
