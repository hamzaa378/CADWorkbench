#include "core/CylindricalFitReview.h"
#include "core/AssemblyHierarchyAudit.h"
#include "core/MotionDriveLimitReview.h"
#include "core/GdtDatumReview.h"
#include "core/BendReliefPlanning.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main(){
 int checks=0;
 const auto test=[&](bool condition){assert(condition);++checks;};
 std::string err;
 // Feature 1: user-identified bore/shaft fits: interval mathematics and fail-closed.
 cad::mates::CylindricalFitReview fit;
 test(cad::mates::reviewCylindricalFit(10.05,10.00,0,0.01,0,0.01,&fit,&err));
 test(std::abs(fit.smallestPossibleClearanceMm-0.04)<1e-8);
 test(std::abs(fit.largestPossibleClearanceMm-0.06)<1e-8);
 test(fit.alwaysClearance&&!fit.alwaysInterference&&!fit.transitionFit);
 test(cad::mates::reviewCylindricalFit(10.0,10.1,0.0,0.0,0,0,&fit,&err)&&fit.alwaysInterference);
 test(cad::mates::reviewCylindricalFit(10,10,-0.02,0.02,-0.02,0.02,&fit,&err)&&fit.transitionFit);
 auto preserved=fit;
 test(!cad::mates::reviewCylindricalFit(10,10,1,-1,0,0,&fit,&err));
 test(fit.transitionFit==preserved.transitionFit&&fit.smallestPossibleClearanceMm==preserved.smallestPossibleClearanceMm);
 test(!cad::mates::reviewCylindricalFit(1,1,-2,0,0,0,&fit,&err));
 test(!cad::mates::reviewCylindricalFit(10,10,0,0,0,0,nullptr,&err));
 test(!cad::mates::reviewCylindricalFit(std::numeric_limits<double>::quiet_NaN(),1,0,0,0,0,&fit,&err));
 // Feature 2: independently authored occurrence hierarchy, no mate-based inference.
 using namespace cad::assembly;
 HierarchyReport hierarchy;
 test(auditHierarchy({"sub-a","root","leaf-b","leaf-c"},{1,-1,0,0},&hierarchy,&err));
 test(hierarchy.rootCount==1 && hierarchy.maximumDepth==2);
 test(hierarchy.depthByOccurrence==std::vector<std::size_t>({1,0,2,2}));
 test(hierarchy.leafIndices==std::vector<std::size_t>({2,3}));
 test(auditHierarchy({"root1","root2"},{-1,-1},&hierarchy,&err)&&hierarchy.rootCount==2);
 auto previous=hierarchy;
 test(!auditHierarchy({"a","b"},{1,0},&hierarchy,&err));
 test(hierarchy.rootCount==previous.rootCount);
 test(!auditHierarchy({"a","a"},{-1,0},&hierarchy,&err));
 test(!auditHierarchy({"a"},{0},&hierarchy,&err));
 test(!auditHierarchy({"a","b"},{-1,3},&hierarchy,&err));
 // Feature 3: analytic drive speed / acceleration envelopes, fail-closed.
 using namespace cad::joint_motion;
 const std::vector<Drive> drives{{1,0,10,-1,12}};
 const std::vector<DriveLimit> limits{{1,8,16}};
 std::vector<DriveEnvelope> envelopes;
 test(reviewDriveLimits(drives,2,Progression::Smoothstep,limits,&envelopes,&err));
 test(envelopes.size()==1 && std::abs(envelopes[0].peakSpeed-7.5)<1e-10);
 test(std::abs(envelopes[0].peakAcceleration-15)<1e-10);
 auto old=envelopes;
 test(!reviewDriveLimits(drives,2,Progression::Smoothstep,{{1,7.0,16}},&envelopes,&err));
 test(envelopes.size()==old.size()&&envelopes[0].peakSpeed==old[0].peakSpeed);
 test(!reviewDriveLimits(drives,2,Progression::Smoothstep,{{1,8,14}},&envelopes,&err));
 test(!reviewDriveLimits(drives,2,Progression::Linear,limits,&envelopes,&err));
 test(reviewDriveLimits({{2,2,2,0,3}},2,Progression::Linear,{{2,0,0}},&envelopes,&err));
 test(envelopes[0].peakSpeed==0&&envelopes[0].peakAcceleration==0);
 test(!reviewDriveLimits(drives,2,Progression::Smoothstep,{{2,8,16}},&envelopes,&err));
 test(!reviewDriveLimits(drives,0,Progression::Smoothstep,limits,&envelopes,&err));
 // Feature 4: GD&T datum semantics are advisory, not unrequested rewriting.
 using namespace cad::drawing;
 std::vector<GdtAdvice> warnings;
 test(reviewGdtDatumPolicy({{"G1","FLATNESS","A","","",0.02},
                            {"G2","POSITION","","","",0.1},
                            {"G3","PARALLELISM","A","","",0.1}},
                           &warnings,&err));
 test(warnings.size()==2&&warnings[0].frameId=="G1"&&warnings[1].frameId=="G2");
 test(reviewGdtDatumPolicy({{"G4","PROFILE","","","",0.05}},&warnings,&err));
 test(warnings.size()==1);
 auto oldWarnings=warnings;
 test(!reviewGdtDatumPolicy({{"G5","POSITION","A","A","",0.1}},&warnings,&err));
 test(warnings.size()==oldWarnings.size());
 test(!reviewGdtDatumPolicy({{"G6","POSITION","","B","",0.1}},&warnings,&err));
 test(!reviewGdtDatumPolicy({{"G7","POSITION","A","","",-1}},&warnings,&err));
 test(reviewGdtDatumPolicy({},&warnings,&err)&&warnings.empty());
 // Feature 5: bend-by-bend relief rule for a user-defined shop policy.
 using namespace cad::sheet;
 std::vector<ReliefDecision> results;
 const std::vector<BendInput> bends{{90,2,0.33},{-90,3,0.33}};
 test(reviewEndReliefs(bends,1.5,50,2.0,4.0,{1,0.5},&results,&err));
 test(results.size()==2 && results[0].widthPass && results[0].depthPass);
 test(results[1].widthPass && results[1].depthPass && results[1].minimumDepthMm==3.5);
 test(reviewEndReliefs(bends,1.5,50,1.0,2.4,{1,0.5},&results,&err));
 test(!results[0].widthPass && !results[1].depthPass);
 auto oldResults=results;
 test(!reviewEndReliefs(bends,1.5,3.0,2,4,{1,0.5},&results,&err));
 test(results.size()==oldResults.size()&&results[0].widthPass==oldResults[0].widthPass);
 test(!reviewEndReliefs(bends,0,50,2,4,{1,0.5},&results,&err));
 test(!reviewEndReliefs({{180,2,0.33}},1.5,50,2,4,{1,0.5},&results,&err));
 test(!reviewEndReliefs(bends,1.5,50,2,4,{std::numeric_limits<double>::infinity(),0.5},&results,&err));
 std::cout<<"PASS: five independently tested engineering developments ("<<checks<<" assertions)\n";
}
