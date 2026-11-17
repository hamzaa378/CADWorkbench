#include "core/GuidedEngineeringStudies.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
int checks=0;
void yes(bool condition,const std::string& name){
    ++checks;if(!condition){std::cerr<<"FAILED "<<name<<'\n';std::exit(1);}
}
}
int main(){
    const auto& studies=cad::guidedEngineeringStudyCatalog();
    yes(studies.size()==11,"eleven engineering studies");
    std::set<std::string> ids;
    for(const auto& tool:studies){
        yes(ids.insert(tool.id).second,"unique study ID");
        yes(!tool.assumptions.empty(),"assumptions present");
        std::vector<double> input;
        for(const auto& x:tool.inputs)input.push_back(x.example);
        const auto result=cad::evaluateGuidedEngineeringStudy(tool.id,input);
        yes(result.valid,"example evaluates: "+tool.id+": "+result.error);
        yes(result.metrics.size()>=2,"multiple results");
        yes(!result.warnings.empty(),"limits warning");
        for(const auto& y:result.metrics)yes(std::isfinite(y.value)&&!y.unit.empty(),"finite labeled metrics");
        yes(!cad::evaluateGuidedEngineeringStudy(tool.id,{}).valid,"wrong arity rejected");
        input[0]=-1;
        yes(!cad::evaluateGuidedEngineeringStudy(tool.id,input).valid,"negative input rejected");
    }
    yes(!cad::evaluateGuidedEngineeringStudy("bogus",{}).valid,"unknown study rejected");
    const auto shaft=cad::evaluateGuidedEngineeringStudy("combined_shaft",{60000,40000,235,2});
    yes(shaft.valid&&shaft.metrics[0].value>10&&shaft.metrics[0].value<30,"reasonable shaft size");
    const auto bolt=cad::evaluateGuidedEngineeringStudy("bolt_group",{10000,4000,4,58,250});
    yes(bolt.valid&&std::abs(bolt.metrics[0].value-10000.0/(4*58))<1e-9,"bolt axial stress");
    yes(!cad::evaluateGuidedEngineeringStudy("bolt_group",{10000,4000,4.5,58,250}).valid,"fractional bolts rejected");
    yes(!cad::evaluateGuidedEngineeringStudy("linear_fit",{20.03,20,19.98,20.01}).valid,"invalid fit limits rejected");
    const auto stack=cad::evaluateGuidedEngineeringStudy("three_part_clearance_stack",
        {20,10,29.4,0.10,0.05,0.15,0.2});
    yes(stack.valid&&stack.metrics.size()==5,"complete deterministic tolerance stack");
    yes(std::abs(stack.metrics[0].value-0.6)<1e-9,"signed dimension equation");
    yes(std::abs(stack.metrics[1].value-0.3)<1e-9,"worst lower clearance");
    yes(std::abs(stack.metrics[2].value-0.9)<1e-9,"worst upper clearance");
    yes(std::abs(stack.metrics[4].value-0.1)<1e-9,"minimum clearance margin");
    const auto unsafe=cad::evaluateGuidedEngineeringStudy("three_part_clearance_stack",
        {20,10,29.4,0.3,0.3,0.3,0.2});
    yes(unsafe.valid&&unsafe.metrics[4].value<0&&unsafe.warnings.size()==2,
        "risky assembly must not be marked safe");
    yes(!cad::evaluateGuidedEngineeringStudy("three_part_clearance_stack",
        {20,10,29.4,-0.1,0.05,0.15,0.2}).valid,"negative deviation rejected");
    const auto bend=cad::evaluateGuidedEngineeringStudy("bend_layout",{90,2,1,0.33});
    yes(bend.valid&&bend.metrics[0].value>3&&bend.metrics[0].value<4,"bend allowance");
    const auto drill=cad::evaluateGuidedEngineeringStudy("peck_drilling",{35,5,0.12,1500,0.8});
    yes(drill.valid&&drill.metrics[0].value==7,"peck count");
    const auto col=cad::evaluateGuidedEngineeringStudy("euler_column",{10000,1500,200,1e6,1});
    yes(col.valid&&col.metrics[0].value>0,"positive buckling capacity");
    for(int i=1;i<=25;++i){
        const auto r=cad::evaluateGuidedEngineeringStudy("cantilever_bracket",{150.,30.,12.,static_cast<double>(i)*100,200.,235.});
        yes(r.valid&&r.metrics[1].value>0,"cantilever load cases");
    }
    std::cout<<"PASS: "<<checks<<" guided engineering workflow checks\n";
}
