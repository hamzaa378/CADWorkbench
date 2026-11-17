#include "WorkshopEngineering.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <utility>

namespace cad::workshop {
namespace {
constexpr double pi=std::numbers::pi;
bool finite(double v){return std::isfinite(v);}
void failure(std::string* error,const std::string& message){if(error)*error=message;}
struct Rect {double x,y,w,h;};
bool validPositive(double value){return finite(value)&&value>0.0;}
bool validateRects(const std::vector<Rect>& rects){
    for(const auto& r:rects){if(!finite(r.x)||!finite(r.y)||!validPositive(r.w)||!validPositive(r.h))return false;}
    return true;
}
bool finiteSection(const SectionResult& s){
    return validPositive(s.areaMm2)&&finite(s.centroidXmm)&&finite(s.centroidYmm)&&
        validPositive(s.secondMomentXmm4)&&validPositive(s.secondMomentYmm4)&&
        finite(s.productMomentXYmm4)&&validPositive(s.polarAreaMomentMm4)&&
        validPositive(s.modulusXmm3)&&validPositive(s.modulusYmm3)&&
        validPositive(s.gyrationXmm)&&validPositive(s.gyrationYmm)&&finite(s.massKgPerMeter);
}
}
std::vector<std::string> sectionProfiles(){
    return {"rectangle","circle","hollow_circle","box","i_beam","channel","tee","angle"};
}
bool calculateSection(const SectionSpec& s,SectionResult* output,std::string* error){
    if(!output){failure(error,"A section result is required.");return false;}
    if(!validPositive(s.widthMm)||!finite(s.heightMm)||!finite(s.wallMm)||!finite(s.flangeMm)||
       !finite(s.densityKgPerM3)||s.densityKgPerM3<0||s.densityKgPerM3>100000){
        failure(error,"Section dimensions and density must be finite and within physical bounds.");return false;
    }
    SectionResult r;
    const double w=s.widthMm,h=s.heightMm,t=s.wallMm,f=s.flangeMm;
    const bool circular=s.profile=="circle"||s.profile=="hollow_circle";
    if(circular){
        if(s.profile=="hollow_circle"&&!(t>0&&t<w/2)){
            failure(error,"Hollow circular wall must be positive and below the radius.");return false;
        }
        const double outer=w/2,inner=s.profile=="circle"?0:outer-t;
        r.areaMm2=pi*(outer*outer-inner*inner);
        r.centroidXmm=outer;r.centroidYmm=outer;
        r.secondMomentXmm4=r.secondMomentYmm4=pi*(std::pow(outer,4)-std::pow(inner,4))/4;
        r.modulusXmm3=r.secondMomentXmm4/outer;r.modulusYmm3=r.modulusXmm3;
    }else{
        if(!validPositive(h)||w>1e7||h>1e7){failure(error,"Section width and height must be positive and bounded.");return false;}
        std::vector<Rect> rects;
        if(s.profile=="rectangle")rects={{0,0,w,h}};
        else if(s.profile=="box"){
            if(!(t>0&&2*t<std::min(w,h))){failure(error,"Box wall thickness must be less than half of both outer dimensions.");return false;}
            rects={{0,0,w,t},{0,h-t,w,t},{0,t,t,h-2*t},{w-t,t,t,h-2*t}};
        }else if(s.profile=="i_beam"){
            if(!(t>0&&t<w&&f>0&&2*f<h)){
                failure(error,"I-section requires web < width and twice flange thickness < height.");return false;
            }
            rects={{0,0,w,f},{0,h-f,w,f},{(w-t)/2,f,t,h-2*f}};
        }else if(s.profile=="channel"){
            if(!(t>0&&t<w&&f>0&&2*f<h)){
                failure(error,"Channel web and flanges must fit inside the outer rectangle.");return false;
            }
            rects={{0,0,t,h},{t,0,w-t,f},{t,h-f,w-t,f}};
        }else if(s.profile=="tee"){
            if(!(t>0&&t<w&&f>0&&f<h)){
                failure(error,"T-section web and flange dimensions are invalid.");return false;
            }
            rects={{(w-t)/2,0,t,h-f},{0,h-f,w,f}};
        }else if(s.profile=="angle"){
            if(!(t>0&&t<w&&f>0&&f<h)){
                failure(error,"Angle leg thicknesses must fit inside outer dimensions.");return false;
            }
            rects={{0,0,t,h},{t,0,w-t,f}};
        }else{failure(error,"Unknown section profile.");return false;}
        if(!validateRects(rects)){failure(error,"Invalid section rectangle decomposition.");return false;}
        double ax=0,ay=0;
        for(const auto& b:rects){const double a=b.w*b.h;r.areaMm2+=a;ax+=a*(b.x+b.w/2);ay+=a*(b.y+b.h/2);}
        r.centroidXmm=ax/r.areaMm2;r.centroidYmm=ay/r.areaMm2;
        for(const auto& b:rects){
            const double a=b.w*b.h,dx=b.x+b.w/2-r.centroidXmm,dy=b.y+b.h/2-r.centroidYmm;
            r.secondMomentXmm4+=b.w*std::pow(b.h,3)/12+a*dy*dy;
            r.secondMomentYmm4+=b.h*std::pow(b.w,3)/12+a*dx*dx;
            r.productMomentXYmm4+=a*dx*dy;
        }
        r.modulusXmm3=r.secondMomentXmm4/std::max(r.centroidYmm,h-r.centroidYmm);
        r.modulusYmm3=r.secondMomentYmm4/std::max(r.centroidXmm,w-r.centroidXmm);
    }
    r.polarAreaMomentMm4=r.secondMomentXmm4+r.secondMomentYmm4;
    r.gyrationXmm=std::sqrt(r.secondMomentXmm4/r.areaMm2);
    r.gyrationYmm=std::sqrt(r.secondMomentYmm4/r.areaMm2);
    r.massKgPerMeter=r.areaMm2*0.001*s.densityKgPerM3*1e-3; // mm² * 1000mm * 1e-9 m³/mm³
    if(!finiteSection(r)){failure(error,"Section calculation overflowed or was degenerate.");return false;}
    *output=r;if(error)error->clear();return true;
}
std::vector<std::string> machiningOperations(){
    return {"drill","ream","tap","counterbore","countersink","mill_slot","mill_face","turn"};
}
bool estimateMachining(const MachiningSpec& s,MachiningResult* output,std::string* error){
    if(!output){failure(error,"Machining output is required.");return false;}
    const auto supported=machiningOperations();
    if(std::find(supported.begin(),supported.end(),s.operation)==supported.end()){
        failure(error,"Unknown machining process.");return false;
    }
    const double a[]={s.toolDiameterMm,s.travelMm,s.cuttingSpeedMMin,s.feedPerRevMm,s.feedPerToothMm,
        s.radialWidthMm,s.depthMm,s.maxSpindleRpm,s.materialDensityKgM3,s.specificCuttingEnergyJMm3,s.availablePowerKw};
    for(const auto v:a)if(!finite(v)||v<0){failure(error,"Machining parameters must be finite and non-negative.");return false;}
    if(!validPositive(s.toolDiameterMm)||!validPositive(s.travelMm)||!validPositive(s.cuttingSpeedMMin)||
       s.passes<1||s.passes>10000||s.teeth<0||s.teeth>1000||s.toolDiameterMm>1e6||s.travelMm>1e8){
        failure(error,"Tool, travel, speed, pass count or tooth count is out of range.");return false;
    }
    const bool milling=s.operation=="mill_slot"||s.operation=="mill_face";
    if(milling){
        if(!validPositive(s.feedPerToothMm)||s.teeth<1||!validPositive(s.radialWidthMm)||!validPositive(s.depthMm)||
            s.radialWidthMm>s.toolDiameterMm){failure(error,"Milling requires teeth, tooth feed and a valid width/depth of cut.");return false;}
    }else if(!validPositive(s.feedPerRevMm)){
        failure(error,"This process requires positive feed per revolution (thread pitch for tapping).");return false;
    }
    MachiningResult r;
    const double desiredRpm=1000*s.cuttingSpeedMMin/(pi*s.toolDiameterMm);
    r.spindleRpm=s.maxSpindleRpm>0?std::min(desiredRpm,s.maxSpindleRpm):desiredRpm;
    r.actualCuttingSpeedMMin=r.spindleRpm*pi*s.toolDiameterMm/1000;
    r.feedMmMin=r.spindleRpm*(milling?s.feedPerToothMm*s.teeth:s.feedPerRevMm);
    r.cycleTimeMin=(s.travelMm*s.passes)/r.feedMmMin;
    if(milling||s.operation=="turn"){
        if(!validPositive(s.radialWidthMm)||!validPositive(s.depthMm)){
            failure(error,"Milling and turning need positive engagement width and depth.");return false;
        }
        r.nominalEngagementVolumeMm3=s.radialWidthMm*s.depthMm*s.travelMm*s.passes;
    }else{
        r.nominalEngagementVolumeMm3=pi*s.toolDiameterMm*s.toolDiameterMm/4*s.travelMm*s.passes;
    }
    r.estimatedRemovalRateMm3Min=r.nominalEngagementVolumeMm3/r.cycleTimeMin;
    r.nominalChipMassKg=r.nominalEngagementVolumeMm3*s.materialDensityKgM3*1e-9;
    r.approximatePowerKw=r.estimatedRemovalRateMm3Min*s.specificCuttingEnergyJMm3/60000;
    const double outputs[]={r.spindleRpm,r.actualCuttingSpeedMMin,r.feedMmMin,r.cycleTimeMin,
        r.nominalEngagementVolumeMm3,r.estimatedRemovalRateMm3Min,r.nominalChipMassKg,r.approximatePowerKw};
    for(double v:outputs)if(!finite(v)||v<0){failure(error,"Machining estimate overflowed.");return false;}
    if(s.maxSpindleRpm>0&&desiredRpm>s.maxSpindleRpm)
        r.warnings.push_back("Requested surface speed exceeds the specified spindle RPM limit; RPM was capped.");
    if(s.availablePowerKw>0&&r.approximatePowerKw>s.availablePowerKw)
        r.warnings.push_back("Estimated cutting power exceeds the entered machine power; review the operation.");
    if(s.specificCuttingEnergyJMm3<=0)
        r.warnings.push_back("Power not estimated: material-specific cutting energy is missing.");
    if(s.materialDensityKgM3<=0)
        r.warnings.push_back("Chip mass not estimated: material density is missing.");
    if(s.operation=="tap")
        r.warnings.push_back("Feed-per-revolution is a thread-pitch assumption; verify tap type, reversal and synchronization.");
    r.warnings.push_back("Nominal cutter-sweep envelope is NOT actual removed stock. No fixtures, approach, retract, tool wear or collisions are modeled.");
    r.warnings.push_back("Not a machine-ready toolpath or G-code. Verify against tooling and machine manufacturer data.");
    *output=std::move(r);if(error)error->clear();return true;
}
bool calculateToleranceStack(const std::vector<ToleranceTerm>& terms,ToleranceResult* output,std::string* error){
    if(!output){failure(error,"A tolerance result is required.");return false;}
    if(terms.empty()||terms.size()>1000){failure(error,"Tolerance chain needs 1 to 1000 terms.");return false;}
    ToleranceResult r;
    double squares=0;
    for(const auto& t:terms){
        if(t.label.empty()||!finite(t.nominalMm)||!finite(t.lowerDeviationMm)||
            !finite(t.upperDeviationMm)||t.lowerDeviationMm>t.upperDeviationMm||
            (t.direction!=1&&t.direction!=-1)){
            failure(error,"Each term needs a label, ordered limits and a +1 or -1 direction.");return false;
        }
        const double sign=t.direction;
        r.nominalMm+=sign*t.nominalMm;
        r.worstMinMm+=sign==1?t.nominalMm+t.lowerDeviationMm:-t.nominalMm-t.upperDeviationMm;
        r.worstMaxMm+=sign==1?t.nominalMm+t.upperDeviationMm:-t.nominalMm-t.lowerDeviationMm;
        r.rssCenterMm+=sign*(t.nominalMm+std::midpoint(t.lowerDeviationMm,t.upperDeviationMm));
        const double half=(t.upperDeviationMm-t.lowerDeviationMm)/2;
        squares+=half*half;
    }
    r.rssHalfWidthMm=std::sqrt(squares);
    if(!finite(r.nominalMm)||!finite(r.worstMinMm)||!finite(r.worstMaxMm)||
       !finite(r.rssCenterMm)||!finite(r.rssHalfWidthMm)){
        failure(error,"Tolerance sum overflowed.");return false;
    }
    r.warnings={"Worst-case limits assume all dimensional extremes can occur simultaneously.",
        "RSS band assumes independent normal variation with each input half-width interpreted as 3 standard deviations; verify process capability."};
    *output=std::move(r);if(error)error->clear();return true;
}
}
