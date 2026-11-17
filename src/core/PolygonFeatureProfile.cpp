#include "PolygonFeatureProfile.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cad::polygon {
namespace {
bool fail(std::string* issue,const char* message){if(issue)*issue=message;return false;}
double cross(const Point& a,const Point& b,const Point& c){
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
bool onSegment(const Point& a,const Point& b,const Point& q){
    constexpr double eps=1e-8;
    return std::abs(cross(a,b,q))<=eps &&
        q.x>=std::min(a.x,b.x)-eps && q.x<=std::max(a.x,b.x)+eps &&
        q.y>=std::min(a.y,b.y)-eps && q.y<=std::max(a.y,b.y)+eps;
}
bool intersect(const Point& a,const Point& b,const Point& c,const Point& d){
    constexpr double eps=1e-8;
    const double x=cross(a,b,c),y=cross(a,b,d),u=cross(c,d,a),v=cross(c,d,b);
    if(((x>eps&&y< -eps)||(x< -eps&&y>eps))&&
       ((u>eps&&v< -eps)||(u< -eps&&v>eps)))return true;
    return (std::abs(x)<=eps&&onSegment(a,b,c)) ||
           (std::abs(y)<=eps&&onSegment(a,b,d)) ||
           (std::abs(u)<=eps&&onSegment(c,d,a)) ||
           (std::abs(v)<=eps&&onSegment(c,d,b));
}
}
double signedArea(const std::vector<Point>& p){
    double sum=0.0;
    for(std::size_t i=0;i<p.size();++i){const auto& a=p[i];const auto& b=p[(i+1)%p.size()];sum+=a.x*b.y-b.x*a.y;}
    return sum*0.5;
}
bool validate(const std::vector<Point>& p,bool radial,std::string* issue){
    if(p.size()<3||p.size()>24)return fail(issue,"A polygon profile needs 3 to 24 vertices.");
    for(const auto& point:p){
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||
           std::abs(point.x)>1e5||std::abs(point.y)>1e5)
            return fail(issue,"Profile coordinates must be finite and within 100000 mm.");
        // Keep the polygon off the axis to avoid degenerate faces at r=0.
        if(radial&&point.x<1e-5)return fail(issue,"Revolve profile radial coordinates must be positive.");
    }
    for(std::size_t i=0;i<p.size();++i){
        const auto& a=p[i];const auto& b=p[(i+1)%p.size()];
        if(std::hypot(a.x-b.x,a.y-b.y)<1e-4)
            return fail(issue,"Profile contains a zero-length or very short edge.");
        if(std::abs(cross(p[(i+p.size()-1)%p.size()],a,b))<1e-7)
            return fail(issue,"Profile has consecutive collinear/degenerate edges.");
    }
    if(std::abs(signedArea(p))<1e-4)return fail(issue,"Profile enclosed area is too small.");
    for(std::size_t i=0;i<p.size();++i){
        const auto& a=p[i];const auto& b=p[(i+1)%p.size()];
        for(std::size_t j=i+1;j<p.size();++j){
            if(j==i+1||(i==0&&j==p.size()-1))continue;
            if(intersect(a,b,p[j],p[(j+1)%p.size()]))
                return fail(issue,"Profile crosses or touches itself.");
        }
    }
    return true;
}
bool validateLoft(const std::vector<Point>& lower,const std::vector<Point>& upper,double height,
                  std::string* issue){
    if(!validate(lower,false,issue)||!validate(upper,false,issue))return false;
    if(lower.size()!=upper.size())return fail(issue,"Loft profiles require the same vertex count.");
    if(!std::isfinite(height)||height<1e-4||height>1e5)
        return fail(issue,"Loft section separation must be positive.");
    if((signedArea(lower)>0)!=(signedArea(upper)>0))
        return fail(issue,"Loft profile winding must match in both sections.");
    // Kernel straight-section loft should not connect zero-length corresponding sides.
    for(std::size_t i=0;i<lower.size();++i){
        if(std::hypot(lower[i].x-upper[i].x,lower[i].y-upper[i].y)>1e5)
            return fail(issue,"Loft section offsets exceed the geometry limit.");
    }
    return true;
}
}
