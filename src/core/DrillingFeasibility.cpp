#include "DrillingFeasibility.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace cad::drilling {
namespace {
bool fail(std::string* error,const char* message){if(error)*error=message;return false;}
bool finite(double v){return std::isfinite(v)&&std::abs(v)<1e6;}
bool idValid(const std::string& id){
    if(id.empty()||id.size()>128)return false;
    for(unsigned char c:id)if(c<32||c==127)return false;
    return true;
}
std::string cell(std::string s){
    const auto first=s.find_first_not_of(" \t\r\n");
    if(first!=std::string::npos&&std::string("=+-@").find(s[first])!=std::string::npos)s.insert(0,"'");
    std::string escaped="\"";
    for(char c:s){if(c=='"')escaped+='"';escaped+=c;}
    escaped+='"';return escaped;
}
std::string fmt(double n){std::ostringstream o;o<<std::fixed<<std::setprecision(4)<<n;return o.str();}
const char* decisionName(Decision d){
    switch(d){case Decision::EligibleForReview:return "REVIEW";
              case Decision::ConditionalFixtureReview:return "FIXTURE REVIEW";
              case Decision::Rejected:return "REJECTED";}
    return "REJECTED";
}
}
bool plan(const Setup& setup,const std::vector<Hole>& holes,
          const std::vector<Tool>& tools,Plan* out,std::string* error){
    if(!out)return fail(error,"A destination is required.");
    if(!idValid(setup.bodyId)||!idValid(setup.geometryRevision)||
       !finite(setup.stockTopZ)||!finite(setup.stockBottomZ)||
       !finite(setup.clearanceZ)||!finite(setup.homeX)||!finite(setup.homeY)||
       !finite(setup.diameterTolerance)||setup.diameterTolerance<0||
       setup.diameterTolerance>2.0||
       setup.stockTopZ-setup.stockBottomZ<=0.001||
       setup.clearanceZ<=setup.stockTopZ+0.1)
        return fail(error,"Invalid stock, clearance, source revision, or diameter tolerance.");
    if(holes.empty()||holes.size()>512||tools.empty()||tools.size()>128)
        return fail(error,"Require 1-512 holes and 1-128 tool definitions.");
    std::set<std::string> names,toolIds;
    for(const auto& tool:tools){
        if(!idValid(tool.id)||!toolIds.insert(tool.id).second||
           !finite(tool.diameter)||!finite(tool.usableLength)||
           tool.diameter<=0||tool.usableLength<=0)
            return fail(error,"Invalid or repeated drill identifier, diameter, or usable length.");
    }
    for(const auto& hole:holes){
        if(!idValid(hole.label)||!names.insert(hole.label).second||
           !finite(hole.x)||!finite(hole.y)||!finite(hole.diameter)||
           !finite(hole.topZ)||!finite(hole.bottomZ)||hole.diameter<=0||
           hole.topZ-hole.bottomZ<=0.0001)
            return fail(error,"Hole identities and dimensions must be finite, positive and unique.");
    }
    Plan candidate;
    candidate.bodyId=setup.bodyId;candidate.geometryRevision=setup.geometryRevision;
    candidate.clearanceZ=setup.clearanceZ;
    candidate.rows.reserve(holes.size());
    for(const auto& hole:holes){
        Row row;row.hole=hole;row.approachZ=setup.clearanceZ;
        row.penetration=setup.stockTopZ-hole.bottomZ;
        if(hole.topZ>setup.stockTopZ+0.01||hole.bottomZ<setup.stockBottomZ-0.01||
           hole.topZ<setup.stockTopZ-0.01){
            row.reason="Not a complete top-entry hole inside the inspected stock envelope.";
        }else if(hole.through && hole.bottomZ>setup.stockBottomZ+0.01){
            row.reason="Inconsistent through-hole and stock-bottom datum.";
        }else if(!hole.through&&hole.bottomZ<=setup.stockBottomZ+0.01){
            row.reason="Blind-hole label conflicts with stock-bottom datum.";
        }else if(hole.diameter<=0.001||row.penetration<=0){
            row.reason="Invalid top-entry penetration.";
        }else{
            // One direct-size drill only. Do not infer pilot/reaming/countersink
            // or reuse a narrower cutter without an explicitly modeled process.
            const Tool* selected=nullptr;
            double bestMismatch=std::numeric_limits<double>::infinity();
            for(const auto& tool:tools){
                const double mismatch=std::abs(tool.diameter-hole.diameter);
                if(mismatch<=setup.diameterTolerance+1e-9&&
                   tool.usableLength+1e-9>=row.penetration&&
                   (mismatch<bestMismatch-1e-12||
                    (std::abs(mismatch-bestMismatch)<1e-12&&
                     (!selected||tool.id<selected->id)))){
                    selected=&tool;bestMismatch=mismatch;
                }
            }
            if(!selected){
                row.reason="No direct-size drill meets diameter tolerance and reach.";
            }else{
                row.toolId=selected->id;
                if(hole.through){
                    // Even after manual confirmation of fixture exit clearance,
                    // no tooling path is approved by a CAD-only audit.
                    row.decision=Decision::ConditionalFixtureReview;
                    row.reason=setup.fixtureExitReviewed?
                        "Fixture exit marked reviewed; operator must still approve process and collision check.":
                        "Fixture exit clearance and breakthrough require independent review.";
                }else{
                    row.decision=Decision::EligibleForReview;
                    row.reason="Geometric/tool feasibility only; no collision, feed, or spindle approval.";
                }
            }
        }
        if(row.decision==Decision::Rejected)++candidate.rejected;
        else if(row.decision==Decision::ConditionalFixtureReview)++candidate.conditional;
        else ++candidate.eligible;
        candidate.rows.push_back(std::move(row));
    }
    // Deterministic nearest-next preview ordering; all approaches are at the
    // explicit clearance plane. Distances are NOT machining time or G-code.
    double cursorX=setup.homeX,cursorY=setup.homeY;
    std::vector<bool> visited(candidate.rows.size(),false);
    for(int number=1;number<=candidate.eligible+candidate.conditional;++number){
        std::size_t best=candidate.rows.size();
        double distance=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<candidate.rows.size();++i){
            const auto& row=candidate.rows[i];
            if(visited[i]||row.decision==Decision::Rejected)continue;
            const double length=std::hypot(row.hole.x-cursorX,row.hole.y-cursorY);
            if(!std::isfinite(length))return fail(error,"Planning coordinate overflow.");
            if(length<distance-1e-9||(std::abs(length-distance)<1e-9&&
               (best==candidate.rows.size()||row.hole.label<candidate.rows[best].hole.label))){
                best=i;distance=length;
            }
        }
        if(best==candidate.rows.size())return fail(error,"Inconsistent planning visitation.");
        visited[best]=true;
        candidate.rows[best].visitOrder=number;
        candidate.estimatedXYTravel+=distance;
        cursorX=candidate.rows[best].hole.x;cursorY=candidate.rows[best].hole.y;
    }
    if(!finite(candidate.estimatedXYTravel))return fail(error,"Planning travel overflow.");
    *out=std::move(candidate);
    if(error)error->clear();return true;
}
bool exportReviewCsv(const Plan& plan,std::string* csv,std::string* error){
    if(!csv)return fail(error,"An output buffer is required.");
    if(!idValid(plan.bodyId)||!idValid(plan.geometryRevision)||
       !finite(plan.clearanceZ)||!finite(plan.estimatedXYTravel)||
       plan.rows.empty()||plan.rows.size()>512||
       plan.eligible<0||plan.conditional<0||plan.rejected<0||
       plan.eligible+plan.conditional+plan.rejected!=static_cast<int>(plan.rows.size()))
        return fail(error,"Malformed or unversioned drill review plan.");
    std::ostringstream text;
    text<<"CADWorkbench drilling feasibility WORKSHEET - NOT machine instructions\r\n"
        <<"Source body,"<<cell(plan.bodyId)<<"\r\n"
        <<"Source geometry revision,"<<cell(plan.geometryRevision)<<"\r\n"
        <<"CAD geometry does not establish material, machine, fixturing, tool path clearance, feeds, or approval\r\n"
        <<"Safe-approach plane Z (mm),"<<fmt(plan.clearanceZ)<<"\r\n"
        <<"Review preview XY travel (mm),"<<fmt(plan.estimatedXYTravel)<<"\r\n"
        <<"Hole,Decision,Review order,Tool ID,X (mm),Y (mm),Diameter (mm),Top Z (mm),Bottom Z (mm),Depth (mm),Reason\r\n";
    for(const Row& row:plan.rows){
        if(!idValid(row.hole.label)||!finite(row.penetration)||
           !finite(row.hole.x)||!finite(row.hole.y)||!finite(row.hole.diameter)||
           !finite(row.hole.topZ)||!finite(row.hole.bottomZ)||
           row.visitOrder<0||row.visitOrder>static_cast<int>(plan.rows.size()))
            return fail(error,"Malformed review row.");
        text<<cell(row.hole.label)<<','<<decisionName(row.decision)<<','<<row.visitOrder<<','
            <<cell(row.toolId)<<','<<fmt(row.hole.x)<<','<<fmt(row.hole.y)<<','
            <<fmt(row.hole.diameter)<<','<<fmt(row.hole.topZ)<<','<<fmt(row.hole.bottomZ)<<','
            <<fmt(row.penetration)<<','<<cell(row.reason)<<"\r\n";
    }
    *csv=text.str();if(error)error->clear();return true;
}
} // namespace cad::drilling
