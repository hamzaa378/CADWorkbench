#include "core/LinkedFlatDevelopmentAudit.h"
#include <numbers>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {long long checks=0;void check(bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);}}
int main(){try {
using namespace cad::sheet;
for(int count=1;count<=10;++count)for(int seed=0;seed<100;++seed){
 std::vector<double> lengths(count+1);std::vector<BendInput> bends;
 for(int i=0;i<=count;++i)lengths[i]=20.+(i*7+seed)%35;
 for(int i=0;i<count;++i)bends.push_back({(i%2?-1.:1.)*(30+(i+seed)%110),1.+i*.1,.25+seed*.001});
 BendSequenceReport ref;std::string issue;LinkedFlatAudit out;
 check(inspectBendSequence(lengths,1.5,bends,&ref,&issue),"valid folded reference");
 std::vector<double> stations,allowances;
 for(const auto& s:ref.stations){stations.push_back(s.stationMm);allowances.push_back(s.neutralAllowanceMm);}
 check(auditLinkedFlatDevelopment(lengths,40,1.5,bends,ref.developedLengthMm,40,stations,allowances,&out,&issue),"valid linked review");
 check(out.bendStationsMm.size()==std::size_t(count)&&!out.releaseApproved,"complete unissued bend review");
 auto prior=out;
 stations[0]+=0.2;
 check(!auditLinkedFlatDevelopment(lengths,40,1.5,bends,ref.developedLengthMm,40,stations,allowances,&out,&issue),"misaligned drawing station rejected");
 check(out.bendStationsMm==prior.bendStationsMm,"failed review atomic");
 stations[0]-=0.2;allowances[0]*=1.01;
 check(!auditLinkedFlatDevelopment(lengths,40,1.5,bends,ref.developedLengthMm,40,stations,allowances,&out,&issue),"altered bend allowance rejected");
 allowances[0]/=1.01;
 check(!auditLinkedFlatDevelopment(lengths,40,1.5,bends,ref.developedLengthMm+2,40,stations,allowances,&out,&issue),"altered blank length rejected");
 check(!auditLinkedFlatDevelopment(lengths,40,1.5,bends,ref.developedLengthMm,41,stations,allowances,&out,&issue),"altered width rejected");
 check(out.bendStationsMm==prior.bendStationsMm,"all failed scenarios preserve report");
}
std::cout<<checks<<" linked-folded production checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
