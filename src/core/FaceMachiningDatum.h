#pragma once
// Select a stable face-local machining origin without depending on the
// centroid of a regenerated trimmed face. Pure geometry-reference decision:
// the caller must verify witness containment on the candidate OCCT face.
// If the unchanged-world witness survives, a coplanar split/merge did not
// move the original design datum. Only when the shifted witness survives do
// we follow an actual body translation. Never guess from a changed centroid.
#include <cmath>
namespace cad::machining_datum {
struct Vec3 { double x=0.0, y=0.0, z=0.0; };
inline bool finite(const Vec3& p){
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
           std::abs(p.x)<=1e9&&std::abs(p.y)<=1e9&&std::abs(p.z)<=1e9;
}
inline bool chooseOrigin(const Vec3& savedOrigin,const Vec3& bodyShift,
                        bool originalWitnessInside,bool shiftedWitnessInside,
                        bool coherentRigidTranslation,Vec3* output){
    if(!output||!finite(savedOrigin)||!finite(bodyShift)||
       (!originalWitnessInside&&!shiftedWitnessInside))return false;
    const bool followTranslation=shiftedWitnessInside&&
        (!originalWitnessInside||coherentRigidTranslation);
    const Vec3 candidate=followTranslation?
        Vec3{savedOrigin.x+bodyShift.x,savedOrigin.y+bodyShift.y,savedOrigin.z+bodyShift.z}:savedOrigin;
    if(!finite(candidate))return false;
    *output=candidate;
    return true;
}
}
