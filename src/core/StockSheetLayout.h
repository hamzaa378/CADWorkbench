#pragma once
// Conservative rectangular envelope placement for sheet-metal blank planning.
// Uses deterministic free-rectangle reuse (MaxRects-style subdivision), not
// contour nesting, certified grain alignment, bend tooling or CNC toolpaths.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cad::stock_layout {
struct Blank { std::string id; double lengthMm=0,widthMm=0; };
struct Placement {
    std::string id;
    std::size_t sheet=0;
    double xMm=0,yMm=0,lengthMm=0,widthMm=0;
    bool rotated=false;
};
struct Plan {
    std::vector<Placement> placements;
    std::size_t sheets=0;
    double stockWidthMm=0,stockHeightMm=0,gapMm=0;
    double totalBlankAreaMm2=0,utilization=0;
};
struct FreeRegion { double x,y,w,h; };
inline bool intersects(const FreeRegion& a,const FreeRegion& b,double eps){
    return a.x<b.x+b.w-eps && b.x<a.x+a.w-eps &&
           a.y<b.y+b.h-eps && b.y<a.y+a.h-eps;
}
inline bool contained(const FreeRegion& a,const FreeRegion& b,double eps){
    return a.x>=b.x-eps && a.y>=b.y-eps &&
           a.x+a.w<=b.x+b.w+eps && a.y+a.h<=b.y+b.h+eps;
}
// Carve an inflated occupied rectangle from every free region, then remove
// contained regions. The regions may overlap each other, but no region ever
// overlaps an occupied item. This allows real reuse of space above/beside
// previously placed blanks without reordering user-specified part IDs.
inline void consume(std::vector<FreeRegion>* regions,const FreeRegion& occupied,double eps){
    std::vector<FreeRegion> pieces;
    for(const auto& free:*regions){
        if(!intersects(free,occupied,eps)){pieces.push_back(free);continue;}
        const double right=free.x+free.w,top=free.y+free.h;
        const double usedRight=occupied.x+occupied.w,usedTop=occupied.y+occupied.h;
        if(occupied.x>free.x+eps)pieces.push_back({free.x,free.y,occupied.x-free.x,free.h});
        if(usedRight<right-eps)pieces.push_back({usedRight,free.y,right-usedRight,free.h});
        if(occupied.y>free.y+eps)pieces.push_back({free.x,free.y,free.w,occupied.y-free.y});
        if(usedTop<top-eps)pieces.push_back({free.x,usedTop,free.w,top-usedTop});
    }
    std::vector<FreeRegion> pruned;
    for(std::size_t i=0;i<pieces.size();++i){
        if(pieces[i].w<=eps||pieces[i].h<=eps)continue;
        bool redundant=false;
        for(std::size_t j=0;j<pieces.size();++j){
            if(i==j)continue;
            if(contained(pieces[i],pieces[j],eps) &&
               (!contained(pieces[j],pieces[i],eps)||j<i)){
                redundant=true;break;
            }
        }
        if(!redundant)pruned.push_back(pieces[i]);
    }
    *regions=std::move(pruned);
}
inline bool build(const std::vector<Blank>& blanks,double stockWidth,double stockHeight,
                  double gap,bool permitRotation,Plan* output,std::string* error=nullptr){
    auto fail=[&](const char* reason){if(error)*error=reason;return false;};
    if(!output||blanks.empty()||blanks.size()>24||
       !std::isfinite(stockWidth)||!std::isfinite(stockHeight)||!std::isfinite(gap)||
       stockWidth<=0||stockHeight<=0||gap<0||gap>=stockWidth/2||gap>=stockHeight/2||
       !std::isfinite(stockWidth*stockHeight))
        return fail("Invalid stock size, edge gap or number of blanks.");
    Plan next;next.stockWidthMm=stockWidth;next.stockHeightMm=stockHeight;next.gapMm=gap;
    const double eps=1e-12*std::max({1.0,stockWidth,stockHeight});
    std::vector<std::vector<FreeRegion>> freeBySheet;
    struct Choice {bool valid=false,rotated=false;std::size_t sheet=0;
                   double x=0,y=0,w=0,h=0,score=0;};
    for(const Blank& blank:blanks){
        if(blank.id.empty()||!std::isfinite(blank.lengthMm)||!std::isfinite(blank.widthMm)||
           blank.lengthMm<=0||blank.widthMm<=0)
            return fail("Every blank requires a unique nonempty ID and finite positive dimensions.");
        for(const auto& existing:next.placements)
            if(existing.id==blank.id)return fail("Duplicate blank identifier.");
        const double area=blank.lengthMm*blank.widthMm;
        if(!std::isfinite(area)||!std::isfinite(next.totalBlankAreaMm2+area))
            return fail("Blank area overflows numeric range.");
        // Validate fit on an empty stock sheet before attempting any placement.
        const auto fitsEmpty=[&](double w,double h){
            return w+2*gap<=stockWidth+eps&&h+2*gap<=stockHeight+eps;
        };
        if(!fitsEmpty(blank.lengthMm,blank.widthMm)&&
           !(permitRotation&&fitsEmpty(blank.widthMm,blank.lengthMm)))
            return fail("A flat blank does not fit the selected stock, even on an empty sheet.");
        Choice best;
        auto visit=[&](std::size_t sheet,const FreeRegion& free,double width,double height,bool rotated){
            const double inflatedWidth=width+gap,inflatedHeight=height+gap;
            if(inflatedWidth>free.w+eps||inflatedHeight>free.h+eps)return;
            const double waste=free.w*free.h-inflatedWidth*inflatedHeight;
            if(!best.valid||sheet<best.sheet||
                (sheet==best.sheet&&(waste<best.score-eps ||
                  (std::abs(waste-best.score)<=eps &&
                   ((!rotated&&best.rotated)||
                    (rotated==best.rotated&&(free.y<best.y-eps||
                       (std::abs(free.y-best.y)<=eps&&free.x<best.x-eps)))))))){
                best={true,rotated,sheet,free.x,free.y,width,height,waste};
            }
        };
        for(std::size_t sheet=0;sheet<freeBySheet.size();++sheet){
            for(const FreeRegion& free:freeBySheet[sheet]){
                visit(sheet,free,blank.lengthMm,blank.widthMm,false);
                if(permitRotation&&blank.lengthMm!=blank.widthMm)
                    visit(sheet,free,blank.widthMm,blank.lengthMm,true);
            }
        }
        if(!best.valid){
            const std::size_t sheet=freeBySheet.size();
            freeBySheet.push_back({{gap,gap,stockWidth-gap,stockHeight-gap}});
            for(const FreeRegion& free:freeBySheet[sheet]){
                visit(sheet,free,blank.lengthMm,blank.widthMm,false);
                if(permitRotation&&blank.lengthMm!=blank.widthMm)
                    visit(sheet,free,blank.widthMm,blank.lengthMm,true);
            }
            if(!best.valid)return fail("Unable to place a blank on a fresh stock sheet.");
        }
        next.placements.push_back({blank.id,best.sheet+1,best.x,best.y,best.w,best.h,best.rotated});
        consume(&freeBySheet[best.sheet],{best.x,best.y,best.w+gap,best.h+gap},eps);
        next.totalBlankAreaMm2+=area;
    }
    next.sheets=freeBySheet.size();
    const double stockArea=stockWidth*stockHeight*static_cast<double>(next.sheets);
    if(!std::isfinite(stockArea)||stockArea<=0)return fail("Stock area overflows numeric range.");
    next.utilization=next.totalBlankAreaMm2/stockArea;
    if(next.utilization>1.0+1e-8)return fail("Generated placements exceed stock area.");
    *output=std::move(next);
    if(error)error->clear();return true;
}
} // namespace cad::stock_layout
