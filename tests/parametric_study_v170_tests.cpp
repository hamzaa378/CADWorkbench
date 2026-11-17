#include "core/ParametricStudy.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace cad::study;
namespace {
int checked=0;
void require(bool ok,const std::string& label){
    ++checked;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}
}
bool almost(double a,double b){return std::abs(a-b)<1e-8*std::max({1.0,std::abs(a),std::abs(b)});}
}
int main(){
    const auto undef=[](const std::string& s){return Evaluation{false,{},"Unresolved: "+s};};
    const std::vector<std::pair<std::string,double>> conversions={
        {"10 mm",10},{"1 cm",10},{"1 m",1000},{"1 in",25.4},{"1 ft",304.8},
        {"2*25mm",50},{"(20 mm + 5 mm)/5",5},{"10 mm / (2)",5},
        {"2 kN",2000},{"1 GPa",1000},{"2kg",0.002},{"3*pi",3*3.14159265358979323846},
        {"180 deg",3.14159265358979323846},{"10 mm * 2",20},{"1 mm + 2 mm",3},
        {"1 m - 10 cm",900},{"2*3*4",24},{"1/(2+3)",0.2},{"-8mm / 4",-2},
        {"+6mm",6},{"0.5in",12.7},{"1e3mm",1000},{"1min",60},{"1MPa",1},
        {"1Pa",0.000001},{"1kPa",0.001},{"1K",1},{"2+3*4",14},{"(2+3)*4",20}};
    for(const auto& [expr,value]:conversions){
        const auto actual=evaluateExpression(expr,undef);
        require(actual.valid,expr+" parses: "+actual.error);
        require(almost(actual.quantity.value,value),expr+" value");
    }
    const std::vector<std::string> invalid={"5mm+2", "1mm+2N", "1/0", "1mm/0mm",
        "1e999", "(3mm", "3mm)", "@", "x", "2**3", "2//3", "12mm + 5 MPa"};
    for(const auto& expr:invalid)require(!evaluateExpression(expr,undef).valid,expr+" should fail");
    const std::map<std::string,Quantity> baseline={{"width",{100,length}},
        {"height",{50,length}},{"depth",{20,length}},{"instances",{3,scalar}}};
    const std::map<std::string,Dimension> dimensions={{"width",length},{"height",length},{"depth",length},
                                                       {"instances",scalar}};
    std::map<std::string,Quantity> values;
    std::string error;
    require(evaluateParameterExpressions(baseline,{{"width","5cm"},{"height","width/2"},
          {"depth","height-5mm"}},dimensions,&values,&error),"dependent edits: "+error);
    require(almost(values.at("width").value,50),"width result");
    require(almost(values.at("height").value,25),"height result");
    require(almost(values.at("depth").value,20),"depth result");
    const auto before=values;
    require(!evaluateParameterExpressions(baseline,{{"width","height"},{"height","width"}},dimensions,&values,&error),"cycle rejection");
    require(values==before,"failed evaluation does not mutate output");
    require(!evaluateParameterExpressions(baseline,{{"width","depth+7MPa"}},dimensions,&values,&error),"unit mismatch rejection");
    require(!evaluateParameterExpressions(baseline,{{"width","2*unknown"}},dimensions,&values,&error),"undefined symbol rejection");
    require(!evaluateParameterExpressions(baseline,{{"z","1mm"}},dimensions,&values,&error),"unknown target rejection");
    require(evaluateParameterExpressions(baseline,{{"instances","4"}},dimensions,&values,&error),"dimensionless counts");
    require(almost(values.at("instances").value,4),"count result");

    const std::vector<FeatureDependency> graph={{"sketch",{}},{"extrude",{"sketch"}},
         {"hole",{"extrude"}},{"fillet",{"hole"}},{"reference",{}},{"boolean",{"fillet","reference"}},
         {"unrelated",{}}};
    auto plan=makeRebuildPlan(graph,{"sketch"});
    require(plan.valid,"graph accepted");
    require(plan.affected==std::vector<std::string>({"sketch","extrude","hole","fillet","boolean"}),"selective dependency propagation");
    plan=makeRebuildPlan(graph,{"reference"});
    require(plan.valid && plan.affected==std::vector<std::string>({"reference","boolean"}),"other branch invalidation");
    require(!makeRebuildPlan({{"a",{"b"}},{"b",{"a"}}},{"a"}).valid,"cycle rejected");
    require(!makeRebuildPlan({{"a",{"absent"}}},{"a"}).valid,"missing source rejected");
    require(!makeRebuildPlan({{"a",{}},{"a",{}}},{"a"}).valid,"duplicate node rejected");
    require(!makeRebuildPlan(graph,{"absent"}).valid,"unknown change rejected");
    // Deep dependency chains must remain safe without recursive graph traversal.
    std::vector<FeatureDependency> deep;
    for(int i=0;i<5000;++i)
        deep.push_back({"feature_"+std::to_string(i),i?std::vector<std::string>{"feature_"+std::to_string(i-1)}:std::vector<std::string>{}});
    const auto largePlan=makeRebuildPlan(deep,{"feature_0"});
    require(largePlan.valid&&largePlan.affected.size()==5000,"iterative planner handles 5000-node chain");
    require(!makeRebuildPlan(deep,{"not_a_feature"}).valid,"deep graph bad ID rejected");
    const auto dimensioned=evaluateExpression("(1 kg)/(1 mm)",undef);
    require(dimensioned.valid&&dimensioned.quantity.unit==cad::study::Dimension{-2,1,2,0},"derived mass per length dimensions");
    require(!evaluateExpression("50 mm + 1 N",undef).valid,"mixed physical units rejected");
    const auto configs=evaluateConfigurations(baseline,dimensions,
       {{"Small",{{"width","80 mm"},{"depth","width/4"}}},
        {"Large",{{"width","120 mm"},{"height","width/2"}}},
        {"Large",{{"width","200 mm"}}},
        {"Broken",{{"depth","bogus"}}}});
    require(configs.size()==4,"configuration count");
    require(configs[0].valid&&almost(configs[0].values.at("depth").value,20),"small configuration");
    require(configs[1].valid&&almost(configs[1].values.at("height").value,60),"large configuration");
    require(!configs[2].valid&&!configs[3].valid,"duplicate and invalid config rejected");
    MaterialRecord steel{"steel_s235","Structural steel","S235","EN 10025","2024",7850,210,235};
    require(validateMaterial(steel),"complete material properties");
    auto bad=steel;bad.source="";require(!validateMaterial(bad),"material without provenance rejected");
    bad=steel;bad.densityKgPerM3=-2;require(!validateMaterial(bad),"invalid density rejected");
    steel.name="=EXFILTRATE";
    const auto csv=materialCatalogCsv({steel});
    require(csv.find("'=EXFILTRATE")!=std::string::npos,"CSV injection neutralization");
    require(std::count(csv.begin(),csv.end(),'\n')==2,"one material exported");
    require(materialCatalogCsv({steel,bad},&error).empty()&&!error.empty(),"invalid material export rejected");
    require(materialCatalogCsv({steel,steel},&error).empty(),"duplicate material IDs rejected");
    for(int i=1;i<=32;++i){
        const auto expr=std::to_string(i)+" mm / 2";
        const auto out=evaluateExpression(expr,undef);
        require(out.valid&&almost(out.quantity.value,i/2.0),"simple dimensional randomized case");
    }
    std::cout<<"PASS: "<<checked<<" portable parameter, configuration, material, graph checks\n";
}
