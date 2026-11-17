#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Authoring conversion for persistent articulated zero-reference frames.
// Row-major matrix R = Rz(yaw) * Ry(pitch) * Rx(roll); angles in radians.
// This is a coordinate editor, not a mechanism solver.
namespace cad::joint_frame {
struct Euler {double yaw=0,pitch=0,roll=0;};
inline bool finiteEuler(Euler e) noexcept {
    return std::isfinite(e.yaw)&&std::isfinite(e.pitch)&&std::isfinite(e.roll)&&
        std::abs(e.yaw)<=3.1415926535897933&&
        std::abs(e.pitch)<=1.5707963267948967+1e-12&&
        std::abs(e.roll)<=3.1415926535897933;
}
inline bool fromEuler(Euler e,std::array<double,9>* r) noexcept {
    if(!r||!finiteEuler(e))return false;
    const double cy=std::cos(e.yaw),sy=std::sin(e.yaw);
    const double cp=std::cos(e.pitch),sp=std::sin(e.pitch);
    const double cr=std::cos(e.roll),sr=std::sin(e.roll);
    *r={{cy*cp,cy*sp*sr-sy*cr,cy*sp*cr+sy*sr,
         sy*cp,sy*sp*sr+cy*cr,sy*sp*cr-cy*sr,
         -sp,cp*sr,cp*cr}};
    return true;
}
inline bool toEuler(const std::array<double,9>& r,Euler* out) noexcept {
    if(!out)return false;
    for(double v:r)if(!std::isfinite(v))return false;
    // Reject a malformed reflection, skew or scaling instead of forcing a
    // matrix to an approximate orientation and silently changing the design.
    for(int row=0;row<3;++row)for(int other=row;other<3;++other){
        double dot=0;
        for(int col=0;col<3;++col)dot+=r[row*3+col]*r[other*3+col];
        if(std::abs(dot-(row==other?1.0:0.0))>1e-6)return false;
    }
    const double det=r[0]*(r[4]*r[8]-r[5]*r[7])-
                     r[1]*(r[3]*r[8]-r[5]*r[6])+
                     r[2]*(r[3]*r[7]-r[4]*r[6]);
    if(std::abs(det-1.0)>1e-6)return false;
    Euler e;
    e.pitch=std::asin(std::clamp(-r[6],-1.0,1.0));
    if(std::abs(std::cos(e.pitch))>1e-8){
        e.yaw=std::atan2(r[3],r[0]);
        e.roll=std::atan2(r[7],r[8]);
    }else{
        // Gimbal lock: select a unique, equivalent zero-roll convention.
        e.yaw=std::atan2(-r[1],r[4]);
        e.roll=0;
    }
    std::array<double,9> reconstructed;
    if(!fromEuler(e,&reconstructed))return false;
    for(int i=0;i<9;++i)if(std::abs(r[i]-reconstructed[i])>1e-6)return false;
    *out=e;
    return true;
}
} // namespace cad::joint_frame
