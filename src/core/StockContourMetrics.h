#pragma once
// Exact perimeter and net-material area of the orthogonal relief-only sheet
// footprints supported by StockContourNesting. This is a CUT-LENGTH ESTIMATE,
// not toolpath ordering, kerf compensation, lead-ins or cut-process planning.
#include "StockContourNesting.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
namespace cad::contour_metrics {
struct Result {double netAreaMm2=0,removedAreaMm2=0,exposedCutLengthMm=0;
    std::size_t materialRectangles=0;};
inline double visibleSpan(double begin,double end,
                          std::vector<std::pair<double,double>> covered){
    std::sort(covered.begin(),covered.end());
    double cursor=begin,visible=0;
    for(const auto& interval:covered){
        const double first=std::clamp(interval.first,begin,end);
        const double last=std::clamp(interval.second,begin,end);
        if(last<=cursor)continue;
        if(first>cursor)visible+=first-cursor;
        cursor=last;
        if(cursor>=end)break;
    }
    return visible+std::max(0.0,end-cursor);
}
inline bool measure(const contour_nesting::Blank& blank,Result* output,
                    std::string* error=nullptr){
    if(!output){if(error)*error="Cut-length result must not be null.";return false;}
    std::vector<contour_nesting::Rect> parts;
    if(!contour_nesting::materialFor(blank,&parts,error))return false;
    const double tol=1e-9*std::max({1.0,blank.length,blank.width});
    Result staged;staged.materialRectangles=parts.size();
    for(std::size_t i=0;i<parts.size();++i){
        const auto& a=parts[i];
        staged.netAreaMm2+=a.w*a.h;
        // Every exposed outer or cutout segment is counted once. Shared
        // internal seams between adjacent decomposition rectangles are
        // subtracted, even when the overlap covers only part of an edge.
        for(int edge=0;edge<4;++edge){
            const bool horizontal=edge<2;
            const double begin=horizontal?a.x:a.y;
            const double end=begin+(horizontal?a.w:a.h);
            const double line=(edge==0?a.y:edge==1?a.y+a.h:edge==2?a.x:a.x+a.w);
            std::vector<std::pair<double,double>> internal;
            for(std::size_t j=0;j<parts.size();++j){
                if(i==j)continue;
                const auto& b=parts[j];
                const double opposite=edge==0?b.y+b.h:edge==1?b.y:
                    edge==2?b.x+b.w:b.x;
                if(std::abs(line-opposite)>tol)continue;
                const double start=horizontal?b.x:b.y;
                const double stop=start+(horizontal?b.w:b.h);
                if(stop>begin+tol&&start<end-tol)internal.emplace_back(start,stop);
            }
            staged.exposedCutLengthMm+=visibleSpan(begin,end,std::move(internal));
        }
    }
    staged.removedAreaMm2=blank.length*blank.width-staged.netAreaMm2;
    if(!std::isfinite(staged.netAreaMm2)||!std::isfinite(staged.exposedCutLengthMm)||
       staged.netAreaMm2<=0||staged.exposedCutLengthMm<=0||
       staged.removedAreaMm2<-tol*std::max(blank.length,blank.width)){
        if(error)*error="Cut-length computation produced invalid dimensions.";return false;
    }
    *output=staged;if(error)error->clear();return true;
}
} // namespace cad::contour_metrics
