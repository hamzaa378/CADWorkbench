#include "core/EngineeringDesignTools.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks=0;
void require(bool ok,const std::string& message){
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
bool near(double actual,double expected){
    return std::abs(actual-expected)<=std::max(1e-9,std::abs(expected)*1e-8);
}
}

int main(){
    try{
        const auto& all=cad::engineeringToolCatalog();
        require(all.size()==50,"Expected 50 real mathematical tools");
        std::map<std::string,int> groups;
        std::set<std::string> identifiers;
        const std::map<std::string,double> goldens = {
            {"rectangle_area",1000.0},
            {"circle_area",314.1592653589793},
            {"annulus_area",392.69908169872417},
            {"rectangle_inertia",720000.0},
            {"circle_inertia",125663.70614359173},
            {"annulus_inertia",117809.72450961724},
            {"cylinder_volume",31415.92653589793},
            {"tube_volume",94247.77960769379},
            {"plate_mass",0.785},
            {"rod_mass",2.466150233067988},
            {"axial_stress",100.0},
            {"axial_strain",0.0005},
            {"axial_elongation",0.5},
            {"bending_stress",20.0},
            {"solid_shaft_shear",63.66197723675814},
            {"tube_shaft_shear",23.505960825879928},
            {"cylinder_hoop_stress",10.0},
            {"cylinder_axial_stress",5.0},
            {"pin_bearing_stress",200.0},
            {"yield_safety_factor",2.5},
            {"simply_center_deflection",0.10416666666666667},
            {"simply_udl_deflection",0.06510416666666667},
            {"cantilever_tip_deflection",1.6666666666666667},
            {"cantilever_udl_deflection",0.625},
            {"simply_center_moment",250000.0},
            {"simply_udl_moment",125000.0},
            {"cantilever_end_moment",1000000.0},
            {"cantilever_tip_slope",0.0025},
            {"euler_column_load",1973920.8802178715},
            {"rect_beam_max_shear",1.875},
            {"iso_bolt_tensile_area",57.98947542534723},
            {"bolt_tensile_stress",172.41379310344828},
            {"bolt_torque_estimate",20.0},
            {"bolt_group_load",2500.0},
            {"bolt_group_shear_stress",31.84713375796178},
            {"fillet_weld_throat",4.242640687119285},
            {"fillet_weld_nominal_shear",23.570226039551585},
            {"minimum_clearance",0.01},
            {"maximum_clearance",0.05},
            {"diametral_interference",0.02},
            {"thermal_expansion",0.6},
            {"thermal_stress_magnitude",120.0},
            {"tolerance_worst_case",0.3},
            {"tolerance_rss",0.17320508075688776},
            {"sheet_bend_allowance",3.659955441432109},
            {"sheet_bend_deduction",2.340044558567891},
            {"drilling_time",0.3},
            {"turning_time",0.625},
            {"milling_time",0.6666666666666666},
            {"spindle_speed",1591.5494309189535}
        };
        require(goldens.size()==50,"Golden result table must contain 50 cases");
        for(const auto& tool:all){
            require(identifiers.insert(tool.id).second,"Duplicate tool ID: "+tool.id);
            require(!tool.assumptions.empty()&&!tool.resultUnit.empty(),"Missing units or assumptions: "+tool.id);
            require(!tool.inputs.empty(),"Missing inputs: "+tool.id);
            ++groups[tool.group];
            std::vector<double> inputs;
            for(const auto& spec:tool.inputs){
                require(!spec.unit.empty()&&spec.minimum<=spec.example&&spec.example<=spec.maximum,
                        "Bad input descriptor: "+tool.id+"/"+spec.key);
                inputs.push_back(spec.example);
            }
            const auto got=cad::evaluateEngineeringTool(tool.id,inputs);
            require(got.valid,"Default fixture failed: "+tool.id+" " + got.error);
            const auto golden=goldens.find(tool.id);
            require(golden!=goldens.end(),"Golden fixture missing: "+tool.id);
            require(near(got.value,golden->second),"Wrong numerical result: "+tool.id+" actual="+std::to_string(got.value));
            require(!cad::evaluateEngineeringTool(tool.id,{}).valid,"Wrong count accepted: "+tool.id);
            inputs[0]=std::numeric_limits<double>::quiet_NaN();
            require(!cad::evaluateEngineeringTool(tool.id,inputs).valid,"NaN accepted: "+tool.id);
        }
        require(groups.size()==5,"Expected five engineering groups");
        for(const auto& [name,count]:groups)
            require(count==10,"Expected ten tools in group "+name);
        require(!cad::evaluateEngineeringTool("nonexistent",{1}).valid,"Unknown tool accepted");
        require(!cad::evaluateEngineeringTool("annulus_area",{20,30}).valid,"Invalid annulus accepted");
        require(!cad::evaluateEngineeringTool("tube_volume",{20,30,100}).valid,"Invalid tube accepted");
        require(!cad::evaluateEngineeringTool("tube_shaft_shear",{10,20,30}).valid,"Invalid hollow shaft accepted");
        require(!cad::evaluateEngineeringTool("iso_bolt_tensile_area",{4,5}).valid,"Invalid bolt geometry accepted");
        require(!cad::evaluateEngineeringTool("bolt_group_load",{100,2.5}).valid,"Fractional fastener count accepted");
        require(!cad::evaluateEngineeringTool("drilling_time",{30,0,1000}).valid,"Zero drill feed accepted");
        require(!cad::evaluateEngineeringTool("cylinder_hoop_stress",{1,100,20}).valid,"Thick-wall pressure model misuse accepted");
        require(!cad::evaluateEngineeringTool("cylinder_axial_stress",{1,100,20}).valid,"Thick-wall axial model misuse accepted");
        require(!cad::evaluateEngineeringTool("thermal_expansion",{12,100,std::numeric_limits<double>::infinity()}).valid,
                "Infinite temperature delta accepted");
        const auto shrink=cad::evaluateEngineeringTool("thermal_expansion",{12,100,-50});
        require(shrink.valid&&near(shrink.value,-0.06),"Thermal contraction sign lost");
        const auto interference=cad::evaluateEngineeringTool("minimum_clearance",{20,20.02});
        require(interference.valid&&near(interference.value,-0.02),"Interference sign lost");
        std::cout<<"PASS "<<checks<<" engineering calculation checks across 50 tools\n";
        return 0;
    }catch(const std::exception& exception){
        std::cerr<<"FAIL check "<<checks<<": "<<exception.what()<<'\n';return 1;
    }
}
