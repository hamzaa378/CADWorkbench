#pragma once

#include <array>
#include <cmath>

// Distinguish a shared final topology from a shared physical DESIGN LOCATION.
// Re-capturing a final edge can erase which upstream branch a point came from.
// A geometric merge is associative only if independently transported witnesses
// agree in world coordinates. Units are millimeters; no nearest-edge fallback.
namespace cad::lineage {
using DesignPoint = std::array<double,3>;
inline bool samePhysicalWitness(const DesignPoint& a,const DesignPoint& b,
                                double toleranceMm=1e-4){
    if(!std::isfinite(toleranceMm)||toleranceMm<=0.0)return false;
    // Squaring tiny tolerances underflows to zero and squaring large ones
    // overflows. The scaled hypot implementation avoids both, so near-zero
    // witness separation cannot be misclassified after B-rep split/merge.
    for(int axis=0;axis<3;++axis)
        if(!std::isfinite(a[axis])||!std::isfinite(b[axis]))return false;
    const double dx=a[0]-b[0],dy=a[1]-b[1],dz=a[2]-b[2];
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz))return false;
    return std::hypot(dx,dy,dz)<=toleranceMm;
}
} // namespace cad::lineage
