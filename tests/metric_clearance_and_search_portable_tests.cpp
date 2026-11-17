#include "core/MetricClearanceHoleTable.h"
#include "core/CommandSearchRank.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {int n=0;void check(bool v,const char* msg){++n;if(!v)throw std::runtime_error(msg);}}
int main(){try{
using namespace cad;
double diameter=-42;
for(const auto& row:metric_holes::kClearanceRows){
 for(int fit=0;fit<3;++fit){
  check(metric_holes::lookup(row.designation,fit,&diameter),"valid ISO dimension lookup");
  check(std::abs(diameter-row.nominalHoleDiameterMm[fit])<1e-9,"precise nominal dimension");
  check(diameter>row.majorDiameterMm,"positive fastener clearance");
  if(fit>0)check(diameter>=row.nominalHoleDiameterMm[fit-1],"monotonic clearance fit");
 }
}
check(metric_holes::lookup("M6",1,&diameter)&&std::abs(diameter-6.6)<1e-9,"M6 medium");
check(metric_holes::lookup("M8",2,&diameter)&&std::abs(diameter-10)<1e-9,"M8 coarse");
const auto saved=diameter;
check(!metric_holes::lookup("M7",1,&diameter)&&diameter==saved,"unsupported size atomic");
check(!metric_holes::lookup("M6",-1,&diameter)&&diameter==saved,"bad fit negative atomic");
check(!metric_holes::lookup("M6",3,&diameter)&&diameter==saved,"bad fit positive atomic");
check(!metric_holes::lookup("M6",1,nullptr),"null destination rejected");
using commandsearch::score;
check(score("Deep groove ball bearing","Create / Mechanical Components","bearing")>0,"bearing finds part");
check(score("Deep groove ball bearing","Create / Mechanical Components","bushing")>0,"synonym finds bearing");
check(score("ISO 273 metric fastener clearance hole","Create / Modeling","bolt hole")>0,"multi term fastener search");
check(score("ISO 273 metric fastener clearance hole","Create / Modeling","metric clearance")>0,"dimension category search");
check(score("ISO 273 metric fastener clearance hole","Create / Modeling","M6 clearance")>0,"M6 nominal fastener search");
check(score("Hex nut","Create / Mechanical Components","nut")>score("Bolt","Create / Mechanical Components","nut"),"exact keyword outranks alias");
check(score("Export drawing DXF","File / Export","dxf")>0,"file search");
check(score("Create revolute joint","Assembly","dxf")==0,"unrelated query excluded");
check(score("Ball bearing","Mechanical Components","ball bearing")>score("Bearing housing","Mechanical Components","ball bearing"),"phrase relevance");
check(score("Hex nut","Create / Mechanical Components","nUT")>0,"case insensitive");
check(score("Gear wheel","Create / Mechanical Components","gear faster")==0,"AND query prevents accidental action");
std::cout<<n<<" metric-clearance/search assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
