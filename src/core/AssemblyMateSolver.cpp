#include "AssemblyMateSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cad::mates {
namespace {
void fail(std::string* error,const char* message){if(error)*error=message;}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&
    std::abs(a.x)<=1e6&&std::abs(a.y)<=1e6&&std::abs(a.z)<=1e6;}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,double b){return {a.x*b,a.y*b,a.z*b};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double norm(Vec3 a){return std::sqrt(dot(a,a));}
Vec3 rotate(const std::array<double,9>& r,Vec3 p){return {
    r[0]*p.x+r[1]*p.y+r[2]*p.z,r[3]*p.x+r[4]*p.y+r[5]*p.z,
    r[6]*p.x+r[7]*p.y+r[8]*p.z};}
Transform pose(const std::array<double,6>& x){
    const Vec3 w{x[0],x[1],x[2]};double angle=norm(w);
    const double a=angle<1e-8?1.0-angle*angle/6.0:std::sin(angle)/angle;
    const double b=angle<1e-8?0.5-angle*angle/24.0:(1.0-std::cos(angle))/(angle*angle);
    const double wx=w.x,wy=w.y,wz=w.z;
    Transform t;
    t.rotation={{1-b*(wy*wy+wz*wz),-a*wz+b*wx*wy,a*wy+b*wx*wz,
                 a*wz+b*wy*wx,1-b*(wx*wx+wz*wz),-a*wx+b*wy*wz,
                 -a*wy+b*wz*wx,a*wx+b*wz*wy,1-b*(wx*wx+wy*wy)}};
    t.translation={x[3],x[4],x[5]};return t;
}
// Cartesian residuals use mm. Normal-vector errors are scaled by 10 mm,
// so a single degree of angular error matters during the solve.
void residuals(const std::vector<Constraint>& constraints,const Transform& t,
               std::vector<double>& residual){
    residual.clear();residual.reserve(constraints.size()*6);
    auto push=[&](Vec3 v){residual.push_back(v.x);residual.push_back(v.y);residual.push_back(v.z);};
    for(const auto& c:constraints){
        const Vec3 pointDifference=sub(add(rotate(t.rotation,c.childPoint),t.translation),c.parentPoint);
        if(c.kind==Kind::Point){push(pointDifference);continue;}
        const Vec3 direction=rotate(t.rotation,c.childDirection);
        push(mul(sub(direction,c.parentDirection),10.0));
        if(c.kind==Kind::CoaxialAxis){
            // Axial sliding is allowed; require the child line to lie on the
            // parent infinite line and its oriented direction to coincide.
            push(sub(pointDifference,mul(c.parentDirection,dot(pointDifference,c.parentDirection))));
        }else{
            // Planar sliding is allowed; require coincident oriented planes.
            residual.push_back(dot(pointDifference,c.parentDirection)-c.offsetMm);
        }
    }
}
double loss(const std::vector<double>& r){double v=0;for(double x:r)v+=x*x;return v;}
bool linearSolve(double a[6][6],double b[6],double result[6],double relativeTolerance){
    double largest=0;for(int i=0;i<6;++i)for(int j=0;j<6;++j)largest=std::max(largest,std::abs(a[i][j]));
    if(largest<=0||!std::isfinite(largest))return false;
    for(int i=0;i<6;++i){
        int pivot=i;for(int j=i+1;j<6;++j)if(std::abs(a[j][i])>std::abs(a[pivot][i]))pivot=j;
        if(std::abs(a[pivot][i])<largest*relativeTolerance)return false;
        for(int k=i;k<6;++k){std::swap(a[pivot][k],a[i][k]);}
        std::swap(b[pivot],b[i]);
        const double d=a[i][i];for(int k=i;k<6;++k)a[i][k]/=d;b[i]/=d;
        for(int j=0;j<6;++j)if(j!=i){
            const double f=a[j][i];for(int k=i;k<6;++k)a[j][k]-=f*a[i][k];b[j]-=f*b[i];
        }
    }
    for(int i=0;i<6;++i){result[i]=b[i];}
    return true;
}
void jacobian(const std::vector<Constraint>& constraints,const std::array<double,6>& x,
              std::vector<double>& baseline,std::vector<std::array<double,6>>& rows){
    residuals(constraints,pose(x),baseline);
    rows.assign(baseline.size(),{});
    for(int j=0;j<6;++j){
        const double delta=j<3?1e-6:1e-4;
        auto plus=x,minus=x;plus[j]+=delta;minus[j]-=delta;
        std::vector<double> a,b;
        residuals(constraints,pose(plus),a);residuals(constraints,pose(minus),b);
        for(std::size_t i=0;i<rows.size();++i)rows[i][j]=(a[i]-b[i])/(2*delta);
    }
}
void normalEquations(const std::vector<double>& r,const std::vector<std::array<double,6>>& j,
                     double h[6][6],double g[6]){
    for(int k=0;k<6;++k){g[k]=0;for(int l=0;l<6;++l)h[k][l]=0;}
    for(std::size_t i=0;i<r.size();++i)for(int k=0;k<6;++k){
        g[k]-=j[i][k]*r[i];for(int l=0;l<6;++l)h[k][l]+=j[i][k]*j[i][l];
    }
}
}
Vec3 apply(const Transform& t,const Vec3& p){return add(rotate(t.rotation,p),t.translation);}
bool solve(const std::vector<Constraint>& constraints,Transform* result,std::string* error){
    if(!result||constraints.empty()||constraints.size()>32){fail(error,"Provide 1-32 mate constraints and an output transform.");return false;}
    for(const auto& c:constraints){
        if(!finite(c.parentPoint)||!finite(c.childPoint)||
           (c.kind!=Kind::Point&&c.kind!=Kind::CoaxialAxis&&c.kind!=Kind::CoincidentPlane)||
           !std::isfinite(c.offsetMm)||std::abs(c.offsetMm)>1e6||
           (c.kind!=Kind::CoincidentPlane&&c.offsetMm!=0.0)){
            fail(error,"Invalid mate coordinates or type.");return false;
        }
        if(c.kind!=Kind::Point&&(!finite(c.parentDirection)||!finite(c.childDirection)||
           std::abs(norm(c.parentDirection)-1)>1e-5||
           std::abs(norm(c.childDirection)-1)>1e-5)){
            fail(error,"Mate axes and normals must be unit length.");return false;
        }
    }
    // Normalize each mate around a corresponding anchor before iterating.
    // This avoids ill-conditioned rotation/translation coupling for assemblies
    // whose global coordinates are hundreds of metres from world zero.
    const Vec3 originParent=constraints.front().parentPoint;
    const Vec3 originChild=constraints.front().childPoint;
    std::vector<Constraint> local=constraints;
    for(auto& constraint:local){
        constraint.parentPoint=sub(constraint.parentPoint,originParent);
        constraint.childPoint=sub(constraint.childPoint,originChild);
    }
    std::array<double,6> x{};
    std::vector<double> r;residuals(local,pose(x),r);
    double current=loss(r),lambda=1e-5;
    for(int iteration=0;iteration<110&&current>1e-12;++iteration){
        std::vector<std::array<double,6>> j;
        jacobian(local,x,r,j);
        double h[6][6],g[6];normalEquations(r,j,h,g);
        bool improved=false;
        for(int attempt=0;attempt<14;++attempt){
            double a[6][6],b[6],step[6];
            for(int k=0;k<6;++k){b[k]=g[k];for(int l=0;l<6;++l)a[k][l]=h[k][l];a[k][k]+=lambda;}
            if(!linearSolve(a,b,step,1e-14))break;
            auto candidate=x;for(int k=0;k<6;++k)candidate[k]+=step[k];
            // Keep the representation bounded; an unconstrained joint must
            // not induce an arbitrarily large rotation or translation.
            const Vec3 omega{candidate[0],candidate[1],candidate[2]};
            if(norm(omega)>std::acos(-1.0)*2.0||!finite({candidate[3],candidate[4],candidate[5]}))break;
            std::vector<double> trial;residuals(local,pose(candidate),trial);
            const double next=loss(trial);
            if(next<current){x=candidate;current=next;r=std::move(trial);lambda=std::max(1e-10,lambda*0.33);improved=true;break;}
            lambda*=10.0;
        }
        if(!improved)break;
    }
    std::vector<std::array<double,6>> j;jacobian(local,x,r,j);
    double h[6][6],g[6];normalEquations(r,j,h,g);
    double dummy[6],a[6][6],b[6];for(int i=0;i<6;++i){b[i]=0;for(int k=0;k<6;++k)a[i][k]=h[i][k];}
    if(!linearSolve(a,b,dummy,1e-9)){fail(error,"Assembly mate system is underconstrained or redundant without six independent degrees of freedom.");return false;}
    // A complete independent six-DOF solution still must satisfy every mate.
    double worst=0;for(double value:r)worst=std::max(worst,std::abs(value));
    if(!std::isfinite(worst)||worst>1e-4){fail(error,"Assembly mates conflict or did not converge to tolerance.");return false;}
    Transform next=pose(x);
    next.translation=add(sub(originParent,rotate(next.rotation,originChild)),next.translation);
    next.maximumResidualMm=worst;
    if(!finite(next.translation)){
        fail(error,"Solved placement is outside supported world coordinate bounds.");return false;
    }
    *result=next;return true;
}
}
