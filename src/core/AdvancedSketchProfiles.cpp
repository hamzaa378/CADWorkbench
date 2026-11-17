#include "AdvancedSketchProfiles.h"
#include <cmath>
#include <algorithm>

namespace cad::advancedprofile {
namespace {
bool reject(std::string* message,const char* why){if(message)*message=why;return false;}
bool finitePoint(const Point3& p){
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::abs(p.x)<=1e5&&std::abs(p.y)<=1e5&&std::abs(p.z)<=1e5;
}
Point3 minus(const Point3& a,const Point3& b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
double square(const Point3& a){return a.x*a.x+a.y*a.y+a.z*a.z;}
double dot(const Point3& a,const Point3& b){return a.x*b.x+a.y*b.y+a.z*b.z;}
}
bool validateSweep(const std::vector<polygon::Point>& profile,
                   const std::vector<Point3>& path,std::string* issue){
    if(!polygon::validate(profile,false,issue))return false;
    if(path.size()<2||path.size()>24)
        return reject(issue,"Sweep requires 2 to 24 3D path vertices.");
    for(const auto& point:path)if(!finitePoint(point))
        return reject(issue,"Sweep path has a non-finite or out-of-range coordinate.");
    double total=0;
    for(std::size_t i=1;i<path.size();++i){
        const Point3 step=minus(path[i],path[i-1]);
        const double length=std::sqrt(square(step));
        if(length<1e-3)return reject(issue,"Sweep path has a zero-length segment.");
        total+=length;
        if(i>1){
            const Point3 before=minus(path[i-1],path[i-2]);
            const double cosine=dot(before,step)/(std::sqrt(square(before))*length);
            if(cosine< -0.97)return reject(issue,"Sweep path has an almost reversing turn.");
        }
    }
    if(total>1e6)return reject(issue,"Sweep path exceeds the safe length limit.");
    // Disallow revisiting non-neighboring path vertices, which typically creates
    // self-intersecting sweeps. Kernel B-rep validation catches other collisions.
    for(std::size_t i=0;i<path.size();++i)for(std::size_t j=i+2;j<path.size();++j)
        if(square(minus(path[i],path[j]))<1e-6)
            return reject(issue,"Sweep path returns to an earlier vertex.");
    return true;
}
bool validateMultiLoft(const std::vector<LoftSection>& sections,std::string* issue){
    if(sections.size()<2||sections.size()>8)
        return reject(issue,"Multi-section loft requires 2 to 8 profile sections.");
    const std::size_t count=sections.front().points.size();
    const bool orientation=polygon::signedArea(sections.front().points)>0;
    for(std::size_t i=0;i<sections.size();++i){
        const auto& s=sections[i];
        if(!std::isfinite(s.z)||std::abs(s.z)>1e5)
            return reject(issue,"Loft section elevation is invalid.");
        if(s.points.size()!=count)
            return reject(issue,"All loft sections require matching vertex counts.");
        if(!polygon::validate(s.points,false,issue))return false;
        if((polygon::signedArea(s.points)>0)!=orientation)
            return reject(issue,"Loft sections must have identical winding.");
        if(i>0&&s.z-sections[i-1].z<1e-3)
            return reject(issue,"Loft elevations must be strictly increasing.");
    }
    return true;
}
bool validateAxisRevolve(const std::vector<polygon::Point>& profile,
                         const Point3& origin,const Point3& direction,
                         double angleDegrees,std::string* issue){
    if(!polygon::validate(profile,true,issue))return false;
    if(!finitePoint(origin)||!finitePoint(direction))
        return reject(issue,"Revolve origin or axis has an invalid coordinate.");
    if(square(direction)<1e-12)
        return reject(issue,"Revolve requires a nonzero axis direction.");
    if(!std::isfinite(angleDegrees)||angleDegrees<1e-3||angleDegrees>360)
        return reject(issue,"Revolve angle must be greater than zero and at most 360 degrees.");
    return true;
}
}
