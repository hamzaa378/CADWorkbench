#pragma once

// Kernel-independent safety check for the faceted cross-section of a folded
// sheet.  This is not a 3-D sheet-metal bend/collision solver. A valid simple
// 2-D profile is necessary, but not sufficient, for an OCCT-valid solid.
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <utility>

namespace cad::detail {
using ContourPoint=std::array<double,2>;

inline long double cross(const ContourPoint& a,const ContourPoint& b,const ContourPoint& c){
    return (static_cast<long double>(b[0])-a[0])*(static_cast<long double>(c[1])-a[1])-
           (static_cast<long double>(b[1])-a[1])*(static_cast<long double>(c[0])-a[0]);
}
inline long double norm(const ContourPoint& a,const ContourPoint& b){
    return std::hypot(static_cast<long double>(b[0])-a[0],static_cast<long double>(b[1])-a[1]);
}
inline int turn(const ContourPoint& a,const ContourPoint& b,const ContourPoint& c){
    const long double product=norm(a,b)*norm(a,c);
    const long double delta=cross(a,b,c);
    const long double eps=1e-14L*product;
    return (delta>eps)?1:((delta<-eps)?-1:0);
}
inline bool onSegment(const ContourPoint& a,const ContourPoint& b,const ContourPoint& p){
    // Do not expand bounding boxes by a global size-scaled tolerance: for long
    // flanges that would mark legitimate close-clearance contours as touching.
    if(turn(a,b,p)!=0)return false;
    return p[0]>=std::min(a[0],b[0])&&p[0]<=std::max(a[0],b[0])&&
           p[1]>=std::min(a[1],b[1])&&p[1]<=std::max(a[1],b[1]);
}
inline bool segmentsTouch(const ContourPoint& a,const ContourPoint& b,
                          const ContourPoint& c,const ContourPoint& d){
    if(std::max(a[0],b[0])<std::min(c[0],d[0])||std::max(c[0],d[0])<std::min(a[0],b[0])||
       std::max(a[1],b[1])<std::min(c[1],d[1])||std::max(c[1],d[1])<std::min(a[1],b[1]))return false;
    const int a1=turn(a,b,c),a2=turn(a,b,d),b1=turn(c,d,a),b2=turn(c,d,b);
    if(a1*a2<0&&b1*b2<0)return true;
    return (a1==0&&onSegment(a,b,c))||(a2==0&&onSegment(a,b,d))||
           (b1==0&&onSegment(c,d,a))||(b2==0&&onSegment(c,d,b));
}

// Offending edge pair is written only on failure.  A failing caller must keep
// its previous geometry and document untouched (atomic edit contract).
inline bool simpleClosedContour(const std::vector<ContourPoint>& vertices,
                                std::pair<std::size_t,std::size_t>* offending=nullptr){
    const std::size_t n=vertices.size();
    if(n<3)return false;
    long double twiceArea=0.0L;
    for(std::size_t i=0;i<n;++i){
        const std::size_t next=(i+1)%n;
        const auto& a=vertices[i];const auto& b=vertices[next];
        if(!std::isfinite(a[0])||!std::isfinite(a[1])||norm(a,b)<1e-10L){
            if(offending)*offending={i,i};
            return false;
        }
        twiceArea+=static_cast<long double>(a[0])*b[1]-static_cast<long double>(b[0])*a[1];
        // An immediate backtrack is an overlapping adjacent edge; it needs a
        // special check because the general pair loop skips adjacent edges.
        const auto& c=vertices[(i+2)%n];
        if(turn(a,b,c)==0 &&
           (static_cast<long double>(b[0])-a[0])*(static_cast<long double>(c[0])-b[0])+
            (static_cast<long double>(b[1])-a[1])*(static_cast<long double>(c[1])-b[1])<0){
            if(offending)*offending={i,next};
            return false;
        }
    }
    if(std::abs(twiceArea)<1e-12L){
        if(offending)*offending={0,n-1};
        return false;
    }
    for(std::size_t i=0;i<n;++i){
        const auto& a=vertices[i];const auto& b=vertices[(i+1)%n];
        for(std::size_t j=i+2;j<n;++j){
            if(i==0&&j==n-1)continue; // same closing vertex
            if(segmentsTouch(a,b,vertices[j],vertices[(j+1)%n])){
                if(offending)*offending={i,j};
                return false;
            }
        }
    }
    return true;
}
} // namespace cad::detail
