#include "core/SheetMetalBendSequenceAudit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {int n=0;void check(bool x,const char* msg){++n;if(!x)throw std::runtime_error(msg);}}
int main(){try{
  using namespace cad::sheet;
  BendSequenceReport out;std::string error;
  const std::vector<double> flanges={100,70,60};
  const std::vector<BendInput> bends={{90,2,.33},{-90,3,.40}};
  check(inspectBendSequence(flanges,2,bends,&out,&error),"two bends valid");
  const double a1=std::numbers::pi/2*(2+.33*2),a2=std::numbers::pi/2*(3+.4*2);
  check(std::abs(out.developedLengthMm-(230+a1+a2))<1e-9,"neutral length conserved");
  check(out.stations.size()==2&&out.stations[0].number==1,"two bend stations");
  check(std::abs(out.stations[0].stationMm-(100+a1/2))<1e-9,"first station");
  check(std::abs(out.stations[1].stationMm-(170+a1+a2/2))<1e-9,"second station");
  check(std::abs(out.stations[0].flatTangentStartMm-100)<1e-9&&
        std::abs(out.stations[0].flatTangentEndMm-(100+a1))<1e-9,
        "first bend marking spans exact neutral allowance");
  check(std::abs(out.stations[1].flatTangentStartMm-(170+a1))<1e-9&&
        std::abs(out.stations[1].flatTangentEndMm-(170+a1+a2))<1e-9,
        "second bend marking starts after intermediate flange");
  check(out.stations[0].flatTangentEndMm<out.stations[1].flatTangentStartMm,
        "adjacent bend neutral-axis regions cannot overlap");
  check(out.reverseDirectionCount==1&&std::abs(out.stations[1].cumulativeDirectionDegrees)<1e-10,"reverse bend sign tracked");
  check(out.minimumStraightFlangeMm==60,"min straight length");
  check(out.flatFlanges.size()==3,"all flat straight flange intervals reported");
  check(std::abs(out.flatFlanges[0].startMm)<1e-12&&
        std::abs(out.flatFlanges[0].endMm-100)<1e-10,"first flange interval");
  check(std::abs(out.flatFlanges[1].startMm-(100+a1))<1e-10&&
        std::abs(out.flatFlanges[1].endMm-(170+a1))<1e-10,"middle flange interval");
  check(std::abs(out.flatFlanges[2].endMm-(230+a1+a2))<1e-10,
        "last flange ends exactly at developed blank end");
  check(std::abs(out.stations[1].insideRadiusToThickness-1.5)<1e-12,"radius ratio");
  BendStationSpacingCheck spacing;
  check(checkBendStationSpacing(out,0,&spacing,&error),"spacing calculated even when shop threshold disabled");
  const double referenceGap=out.stations[1].stationMm-out.stations[0].stationMm;
  check(std::abs(spacing.actualMinimumMm-referenceGap)<1e-9,"spacing is measured between actual bend centers");
  check(spacing.tooClosePairs.empty(),"zero shop threshold never reports a violation");
  check(checkBendStationSpacing(out,referenceGap+1,&spacing,&error),"shop threshold reviewed");
  check(spacing.tooClosePairs==std::vector<std::size_t>{1},"bend pair one and two identified");
  check(checkBendStationSpacing(out,referenceGap-1,&spacing,&error),"roomy limit reviewed");
  check(spacing.tooClosePairs.empty(),"sufficient spacing has no failing pairs");
  auto savedSpacing=spacing;
  check(!checkBendStationSpacing(out,-1,&spacing,&error),"negative shop threshold rejected");
  check(spacing.actualMinimumMm==savedSpacing.actualMinimumMm,"failed spacing audit atomic");
  check(!checkBendStationSpacing(out,INFINITY,&spacing,&error),"infinite shop threshold rejected");
  check(!checkBendStationSpacing(out,2,nullptr,&error),"null output rejected");
  BendSequenceReport bad=out;
  bad.stations[1].stationMm=bad.stations[0].stationMm;
  check(!checkBendStationSpacing(bad,1,&spacing,&error),"reordered station rejected");
  bad=out;bad.stations[0].flatTangentEndMm=bad.stations[1].flatTangentStartMm+1;
  check(!checkBendStationSpacing(bad,1,&spacing,&error),"overlapping bend zones rejected");
  auto saved=out;
  check(!inspectBendSequence(flanges,2,{{90,2,.33}},&out,&error),"mismatched lengths rejected");
  check(out.stations.size()==saved.stations.size(),"invalid audit atomic");
  check(!inspectBendSequence(flanges,-1,bends,&out,&error),"negative thickness rejected");
  check(!inspectBendSequence(flanges,2,{{0,2,.33},{-90,3,.4}},&out,&error),"zero angle rejected");
  check(!inspectBendSequence(flanges,2,{{90,2,-.1},{-90,3,.4}},&out,&error),"bad K rejected");
  check(!inspectBendSequence(flanges,2,{{90,0,.33},{-90,3,.4}},&out,&error),"zero radius rejected");
  check(!inspectBendSequence({100,0,60},2,bends,&out,&error),"zero flange rejected");
  check(!inspectBendSequence({30,30},2,{},&out,&error),"empty bends rejected");
  check(inspectBendSequence({30,30},2,{{165,1,.3}},&out,&error),"maximum angle valid");
  check(out.maximumAbsoluteCumulativeDirectionDegrees==165,"max direction");
  std::cout<<n<<" sheet-bend-sequence assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
