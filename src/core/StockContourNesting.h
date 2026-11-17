#pragma once
// Exact planar overlap checking of axis-aligned rectangular material regions
// for neutral developed sheet blanks with orthogonal end-relief notches.
// Greedy deterministic placement; not optimum CNC nesting, grain alignment,
// kerf compensation or manufacturability verification.
#include "StockSheetLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace cad::contour_nesting {
struct Rect {double x=0,y=0,w=0,h=0;};
struct Blank {std::string id;double length=0,width=0;std::vector<Rect> notches;};
struct Item {stock_layout::Placement placement;std::vector<Rect> material;};
struct Layout {stock_layout::Plan plan;std::vector<Item> items;};
inline bool validRect(const Rect& r){return std::isfinite(r.x)&&std::isfinite(r.y)&&
    std::isfinite(r.w)&&std::isfinite(r.h)&&r.w>0&&r.h>0;}
inline bool intersect(const Rect& a,const Rect& b,double gap,double eps){
    return a.x < b.x+b.w+gap-eps && b.x < a.x+a.w+gap-eps &&
           a.y < b.y+b.h+gap-eps && b.y < a.y+a.h+gap-eps;
}
inline std::vector<Rect> removeRect(const std::vector<Rect>& material,const Rect& notch){
    std::vector<Rect> output;
    for(const Rect& body:material){
        const double x0=std::max(body.x,notch.x),x1=std::min(body.x+body.w,notch.x+notch.w);
        const double y0=std::max(body.y,notch.y),y1=std::min(body.y+body.h,notch.y+notch.h);
        if(x0>=x1||y0>=y1){output.push_back(body);continue;}
        if(x0>body.x)output.push_back({body.x,body.y,x0-body.x,body.h});
        if(x1<body.x+body.w)output.push_back({x1,body.y,body.x+body.w-x1,body.h});
        if(y0>body.y)output.push_back({x0,body.y,x1-x0,y0-body.y});
        if(y1<body.y+body.h)output.push_back({x0,y1,x1-x0,body.y+body.h-y1});
    }
    return output;
}
inline bool materialFor(const Blank& input,std::vector<Rect>* output,std::string* error=nullptr){
    auto fail=[&](const char* detail){if(error)*error=detail;return false;};
    if(!output||input.id.empty()||!std::isfinite(input.length)||!std::isfinite(input.width)||
       input.length<=0||input.width<=0||input.notches.size()>96)
        return fail("Contour requires finite positive envelope, ID and <=96 relief cutouts.");
    std::vector<Rect> result{{0,0,input.length,input.width}};
    const double eps=1e-10*std::max({1.0,input.length,input.width});
    for(const Rect& notch:input.notches){
        if(!validRect(notch)||notch.x<-eps||notch.y<-eps||
           notch.x+notch.w>input.length+eps||notch.y+notch.h>input.width+eps)
            return fail("Notch must be a finite positive rectangle fully inside the developed blank.");
        result=removeRect(result,notch);
        if(result.size()>512)return fail("Excessive contour complexity.");
    }
    double area=0;for(const Rect& r:result)area+=r.w*r.h;
    if(result.empty()||!std::isfinite(area)||area<=eps*eps)
        return fail("Reliefs consumed or corrupted all material.");
    *output=std::move(result);if(error)error->clear();return true;
}
inline std::vector<Rect> rotated(const std::vector<Rect>& original,double sourceHeight){
    std::vector<Rect> output;output.reserve(original.size());
    for(const Rect& r:original)output.push_back({sourceHeight-r.y-r.h,r.x,r.h,r.w});
    return output;
}
inline std::vector<Rect> global(const std::vector<Rect>& local,double x,double y){
    std::vector<Rect> result;result.reserve(local.size());
    for(Rect r:local){r.x+=x;r.y+=y;result.push_back(r);}return result;
}
// Generates candidates at MATERIAL edges and notch corners, not only outside
// bounding boxes; this permits a blank to occupy a different blank's relief.
inline std::vector<double> positions(double limit,double span,double edge,
        const std::vector<Rect>& placed,const std::vector<Rect>& incoming,bool horizontal,
        double gap,double eps){
    std::vector<double> positions{edge,limit-edge-span};
    for(const Rect& other:placed){
        const double p0=horizontal?other.x:other.y;
        const double p1=p0+(horizontal?other.w:other.h);
        for(const Rect& piece:incoming){
            const double s0=horizontal?piece.x:piece.y;
            const double s1=s0+(horizontal?piece.w:piece.h);
            positions.push_back(p1+gap-s0);
            positions.push_back(p0-gap-s1);
            positions.push_back(p0-s0);
            positions.push_back(p1-s1);
        }
    }
    positions.erase(std::remove_if(positions.begin(),positions.end(),[&](double n){
        return !std::isfinite(n)||n<edge-eps||n+span>limit-edge+eps;
    }),positions.end());
    for(double& n:positions)n=std::clamp(n,edge,limit-edge-span);
    std::sort(positions.begin(),positions.end());
    positions.erase(std::unique(positions.begin(),positions.end(),[&](double a,double b){
        return std::abs(a-b)<eps;
    }),positions.end());
    // Deterministic bound on search complexity; deprioritize high coordinates.
    if(positions.size()>320)positions.resize(320);
    return positions;
}
inline bool build(const std::vector<Blank>& blanks,double stockWidth,double stockHeight,
                  double gap,bool rotation,Layout* output,std::string* error=nullptr){
    auto fail=[&](const char* reason){if(error)*error=reason;return false;};
    if(!output||blanks.empty()||blanks.size()>24||
       !std::isfinite(stockWidth)||!std::isfinite(stockHeight)||!std::isfinite(gap)||
       stockWidth<=0||stockHeight<=0||gap<0||gap>=stockWidth/2||gap>=stockHeight/2)
        return fail("Invalid contour stock dimensions, gap, output or number of blanks.");
    std::set<std::string> ids;
    const double eps=1e-9*std::max({1.0,stockWidth,stockHeight});
    std::vector<std::vector<Rect>> shapes;shapes.reserve(blanks.size());
    for(const Blank& blank:blanks){
        if(!ids.insert(blank.id).second)return fail("Duplicate contour blank ID.");
        std::vector<Rect> pieces;
        if(!materialFor(blank,&pieces,error))return false;
        shapes.push_back(std::move(pieces));
        if((blank.length+2*gap>stockWidth+eps||blank.width+2*gap>stockHeight+eps)&&
           (!rotation||(blank.width+2*gap>stockWidth+eps||blank.length+2*gap>stockHeight+eps)))
            return fail("A contour blank cannot fit the stock envelope at either permitted orientation.");
    }
    Layout result;
    result.plan.stockWidthMm=stockWidth;result.plan.stockHeightMm=stockHeight;
    result.plan.gapMm=gap;
    for(std::size_t i=0;i<blanks.size();++i){
        const Blank& blank=blanks[i];
        struct Choice{bool valid=false,rotated=false;std::size_t sheet=0;
            double x=0,y=0;std::vector<Rect> material;};
        Choice chosen;
        // Prefer an earlier sheet; then low y, low x. No preexisting sheet
        // is modified if a future blank cannot be placed.
        for(std::size_t sheet=1;sheet<=result.plan.sheets+1&&!chosen.valid;++sheet){
            std::vector<Rect> already;
            for(const Item& item:result.items)if(item.placement.sheet==sheet)
                already.insert(already.end(),item.material.begin(),item.material.end());
            for(int orientation=0;orientation<(rotation?2:1);++orientation){
                const bool rotate=orientation==1;
                const double w=rotate?blank.width:blank.length;
                const double h=rotate?blank.length:blank.width;
                if(w+2*gap>stockWidth+eps||h+2*gap>stockHeight+eps)continue;
                const auto local=rotate?rotated(shapes[i],blank.width):shapes[i];
                const auto xx=positions(stockWidth,w,gap,already,local,true,gap,eps);
                const auto yy=positions(stockHeight,h,gap,already,local,false,gap,eps);
                // Search in bottom-left order. Bounding boxes MAY overlap, but
                // real material rectangles must retain the physical gap.
                for(double y:yy){
                    for(double x:xx){
                        const auto proposed=global(local,x,y);
                        bool overlaps=false;
                        for(const Rect& a:proposed){
                            for(const Rect& b:already)if(intersect(a,b,gap,eps)){
                                overlaps=true;break;
                            }
                            if(overlaps)break;
                        }
                        if(overlaps)continue;
                        if(!chosen.valid||y<chosen.y-eps||
                           (std::abs(y-chosen.y)<eps&&x<chosen.x-eps))
                            chosen={true,rotate,sheet,x,y,proposed};
                        break;
                    }
                    if(chosen.valid&&y>chosen.y+eps)break;
                }
            }
        }
        if(!chosen.valid)return fail("Contour placement failed on a fresh sheet.");
        stock_layout::Placement placement{blank.id,chosen.sheet,chosen.x,chosen.y,
            chosen.rotated?blank.width:blank.length,
            chosen.rotated?blank.length:blank.width,chosen.rotated};
        result.items.push_back({placement,std::move(chosen.material)});
        result.plan.placements.push_back(placement);
        result.plan.sheets=std::max(result.plan.sheets,chosen.sheet);
        for(const Rect& piece:shapes[i])result.plan.totalBlankAreaMm2+=piece.w*piece.h;
    }
    const double area=stockWidth*stockHeight*static_cast<double>(result.plan.sheets);
    result.plan.utilization=result.plan.totalBlankAreaMm2/area;
    if(!std::isfinite(result.plan.utilization)||result.plan.utilization>1.0+1e-8)
        return fail("Contour utilization is not finite or exceeds stock area.");
    *output=std::move(result);if(error)error->clear();return true;
}
} // namespace cad::contour_nesting
