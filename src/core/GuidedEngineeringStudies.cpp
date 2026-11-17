#include "GuidedEngineeringStudies.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <numbers>
#include <string>
#include <utility>

namespace cad {
namespace {
using Inputs=std::vector<double>;
using Calculation=std::function<GuidedStudyResult(const Inputs&)>;
struct Entry {GuidedStudy study;Calculation evaluate;};
StudyInput in(std::string key,std::string label,std::string unit,double ex,double min=1e-9,double max=1e12){
    return {std::move(key),std::move(label),std::move(unit),ex,min,max};
}
GuidedStudyResult ok(std::initializer_list<StudyMetric> metrics,std::vector<std::string> warnings={}){
    return {true,metrics,std::move(warnings),{}};
}
GuidedStudyResult invalid(const std::string& error){return {false,{},{},error};}
std::vector<std::string> checkUtilization(double utilization){
    if(utilization>1.0)return {"Preliminary utilization exceeds 1.0; revise section/material/loading."};
    return {"Utilization excludes fatigue, stress concentrations, connections and required code factors."};
}
const std::vector<Entry>& registry(){
    static const std::vector<Entry> entries=[] {
        std::vector<Entry> out;
        const auto add=[&](std::string id,std::string name,std::string group,std::string assumptions,
            std::vector<StudyInput> inputs,Calculation fn){
            out.push_back({{std::move(id),std::move(name),std::move(group),std::move(assumptions),std::move(inputs)},std::move(fn)});
        };
        constexpr double pi=std::numbers::pi;
        add("cantilever_bracket","Rectangular cantilever bracket","Structure",
            "Linearly elastic rectangular cantilever with one end fixed and point load at free end; no stress risers.",
            {in("length_mm","Projection","mm",150),in("width_mm","Width","mm",30),
             in("height_mm","Bending thickness","mm",12),in("load_N","Tip load","N",250),
             in("E_GPa","Young's modulus","GPa",200),in("yield_MPa","Yield strength","MPa",235)},
            [](const Inputs& a){
                const double I=a[1]*std::pow(a[2],3)/12;
                const double moment=a[3]*a[0];
                const double bending=moment*(a[2]/2)/I;
                const double deflection=a[3]*std::pow(a[0],3)/(3*a[4]*1000*I);
                const double use=bending/a[5];
                return ok({{"Maximum bending moment","N mm",moment},{"Maximum bending stress","MPa",bending},
                    {"Tip deflection","mm",deflection},{"Yield utilization","ratio",use}},checkUtilization(use));
            });
        add("uniform_beam","Simply supported beam, uniformly distributed load","Structure",
            "Linear-elastic Euler-Bernoulli beam with simple supports and uniform load along the full span.",
            {in("length_mm","Beam span","mm",1000),in("width_mm","Width","mm",30),
             in("height_mm","Bending depth","mm",60),in("load_Nmm","Distributed load","N/mm",0.5),
             in("E_GPa","Young's modulus","GPa",200),in("yield_MPa","Yield strength","MPa",235)},
            [](const Inputs& a){
                const double I=a[1]*std::pow(a[2],3)/12;
                const double M=a[3]*a[0]*a[0]/8;
                const double sigma=M*(a[2]/2)/I;
                const double def=5*a[3]*std::pow(a[0],4)/(384*a[4]*1000*I);
                const double use=sigma/a[5];
                return ok({{"Maximum moment","N mm",M},{"Bending stress","MPa",sigma},
                    {"Midspan deflection","mm",def},{"Yield utilization","ratio",use}},checkUtilization(use));
            });
        add("combined_shaft","Solid circular shaft, combined bending and torsion","Rotating machinery",
            "Static elastic von Mises approximation for nominal solid shaft; omits keyways, fillets and fatigue.",
            {in("bending_Nmm","Bending moment","N mm",60000,0),in("torque_Nmm","Torque","N mm",40000,0),
             in("yield_MPa","Yield strength","MPa",235),in("safety_factor","Required safety factor","ratio",2,1,20)},
            [](const Inputs& a){
                const double equivalent=std::sqrt(std::pow(32*a[0],2)+3*std::pow(16*a[1],2));
                if(equivalent==0)return invalid("At least one load must be positive.");
                const double d=std::cbrt(a[3]*equivalent/(pi*a[2]));
                return ok({{"Minimum nominal shaft diameter","mm",d},
                    {"Nominal combined-load coefficient","N mm",equivalent}},
                    {"Increase nominal diameter for shoulder transitions, keyways, fatigue and manufacturing allowances."});
            });
        add("bolt_group","Axial and shear bolt group","Joints",
            "Identical bolts share axial/shear loads equally; effective tensile area provided by engineer.",
            {in("axial_N","Total axial load","N",10000,0),in("shear_N","Total shear load","N",4000,0),
             in("count","Bolt count","count",4,1,10000),in("area_mm2","Tensile stress area per bolt","mm²",58),
             in("allowable_MPa","Allowable combined stress","MPa",250)},
            [](const Inputs& a){
                if(std::floor(a[2])!=a[2])return invalid("Bolt count must be an integer.");
                const double axial=a[0]/(a[2]*a[3]);
                const double shear=a[1]/(a[2]*a[3]);
                const double vm=std::hypot(axial,std::sqrt(3.0)*shear);
                const double use=vm/a[4];
                return ok({{"Per-bolt axial stress","MPa",axial},{"Per-bolt average shear stress","MPa",shear},
                    {"Combined von Mises stress","MPa",vm},{"Utilization","ratio",use}},checkUtilization(use));
            });
        add("double_fillet_weld","Double fillet weld, direct shear","Welds",
            "Two identical straight fillet welds under in-plane direct shear; throat = 0.707 x leg.",
            {in("load_N","Applied shear","N",5000),in("length_mm","Length of each weld","mm",60),
             in("leg_mm","Fillet leg","mm",5),in("allowable_MPa","Allowable throat shear","MPa",90)},
            [](const Inputs& a){
                const double throat=0.7071067811865475*a[2];
                const double area=2*a[1]*throat;
                const double stress=a[0]/area;
                const double use=stress/a[3];
                return ok({{"Effective throat","mm",throat},{"Total weld throat area","mm²",area},
                           {"Nominal shear stress","MPa",stress},{"Utilization","ratio",use}},checkUtilization(use));
            });
        add("double_shear_pin","Pin in double shear","Joints",
            "Uniform shear on two planes with no bending, bearing, pin clearance or stress concentration.",
            {in("load_N","Transverse pin load","N",12000),in("diameter_mm","Pin diameter","mm",12),
             in("allowable_MPa","Allowable shear stress","MPa",140)},
            [](const Inputs& a){
                const double area=2*pi*a[1]*a[1]/4;
                const double tau=a[0]/area;
                const double use=tau/a[2];
                return ok({{"Total nominal shear area","mm²",area},{"Nominal shear stress","MPa",tau},
                           {"Utilization","ratio",use}},checkUtilization(use));
            });
        add("euler_column","Elastic Euler column stability","Structure",
            "Ideal, straight, slender member. Euler buckling is inapplicable to short/inelastic columns.",
            {in("load_N","Applied compressive load","N",10000),in("length_mm","Unsupported length","mm",1500),
             in("E_GPa","Young's modulus","GPa",200),in("I_mm4","Minimum second moment","mm^4",1000000),
             in("K","Effective length factor","ratio",1,0.1,10)},
            [](const Inputs& a){
                const double pcrit=pi*pi*a[2]*1000*a[3]/std::pow(a[4]*a[1],2);
                const double use=a[0]/pcrit;
                return ok({{"Elastic critical load","N",pcrit},{"Buckling utilization","ratio",use}},
                    {"Check slenderness, geometric imperfections, residual stress and inelastic capacity."});
            });
        add("linear_fit","Worst-case diametral shaft/hole fit","Manufacturing",
            "Tolerance limits are supplied directly; does not infer standardized ISO fit zones.",
            {in("hole_min_mm","Minimum hole","mm",20),in("hole_max_mm","Maximum hole","mm",20.03),
             in("shaft_min_mm","Minimum shaft","mm",19.98),in("shaft_max_mm","Maximum shaft","mm",20.01)},
            [](const Inputs& a){
                if(a[1]<a[0]||a[3]<a[2])return invalid("Maximum limit must be >= minimum limit.");
                const double min=a[0]-a[3],max=a[1]-a[2];
                return ok({{"Minimum clearance (negative = interference)","mm",min},
                           {"Maximum clearance","mm",max},
                           {"Fit range","mm",max-min}},
                           {"Actual fit depends on manufacturing process, surface finish, temperature and GD&T."});
            });
        add("three_part_clearance_stack","Three-contributor clearance tolerance stack","Manufacturing",
            "Signed one-dimensional chain A + B - C, with independently bounded symmetric deviations. "
            "Absolute sum is a deterministic worst case, not a statistical probability or GD&T inspection.",
            {in("span_a_mm","Adding span A","mm",20,0,1e6),
             in("span_b_mm","Adding span B","mm",10,0,1e6),
             in("span_c_mm","Closing span C","mm",29.4,0,1e6),
             in("deviation_a_mm","Maximum |A deviation|","mm",0.10,0,1e4),
             in("deviation_b_mm","Maximum |B deviation|","mm",0.05,0,1e4),
             in("deviation_c_mm","Maximum |C deviation|","mm",0.15,0,1e4),
             in("required_clearance_mm","Required minimum clearance","mm",0.2,0,1e6)},
            [](const Inputs& a){
                const double nominal=a[0]+a[1]-a[2];
                const double worst=a[3]+a[4]+a[5];
                const double low=nominal-worst,high=nominal+worst;
                const double margin=low-a[6];
                std::vector<std::string> warnings={
                    "Only collinear dimensions and symmetric bounded deviations are modeled; "
                    "thermal expansion, correlated geometry, angular errors and datum shifts are excluded."};
                if(margin<0)
                    warnings.push_back("Minimum clearance is NOT guaranteed at worst-case limits; revise the design.");
                else warnings.push_back("Required clearance holds within the stated independent deviation bounds.");
                return ok({{"Nominal signed clearance","mm",nominal},
                    {"Worst-case minimum clearance","mm",low},
                    {"Worst-case maximum clearance","mm",high},
                    {"Maximum deviation envelope","mm",worst},
                    {"Worst-case clearance margin","mm",margin}},std::move(warnings));
            });
        add("bend_layout","Sheet-metal bend layout","Manufacturing",
            "Single bend neutral-axis estimate; correct K-factor requires material/tool/process validation.",
            {in("angle_deg","Bend angle","degrees",90,0.1,170),in("radius_mm","Inside radius","mm",2),
             in("thickness_mm","Thickness","mm",1),in("K","Neutral-axis K-factor","ratio",0.33,0,1)},
            [](const Inputs& a){
                const double theta=a[0]*pi/180;
                const double allowance=theta*(a[1]+a[3]*a[2]);
                const double outside=2*(a[1]+a[2])*std::tan(theta/2);
                return ok({{"Bend allowance","mm",allowance},
                           {"Outside setback total","mm",outside},
                           {"Bend deduction","mm",outside-allowance}},
                           {"Validate the deduction against brake tooling and production samples."});
            });
        add("peck_drilling","Drilling cycle with peck overhead","Manufacturing",
            "Approximate feed-time plus programmed retractions; not a machine-specific verified toolpath.",
            {in("depth_mm","Hole depth","mm",35),in("peck_mm","Maximum peck step","mm",5),
             in("feed_mm_rev","Feed per revolution","mm/rev",0.12),in("rpm","Spindle speed","rpm",1500),
             in("overhead_s","Extra time per peck","s",0.8,0,1000)},
            [](const Inputs& a){
                const double count=std::ceil(a[0]/a[1]);
                if(count>1e6)return invalid("Too many pecks for the supported estimate.");
                const double time=a[0]/(a[2]*a[3])+count*a[4]/60;
                return ok({{"Peck operations","count",count},{"Feed and overhead time","min",time}},
                           {"Tool approach/retract travel and rapid-motion simulation are not included."});
            });
        return out;
    }();
    return entries;
}
}
const std::vector<GuidedStudy>& guidedEngineeringStudyCatalog(){
    static const auto out=[] {
        std::vector<GuidedStudy> studies;
        for(const auto& e:registry())studies.push_back(e.study);
        return studies;
    }();return out;
}
GuidedStudyResult evaluateGuidedEngineeringStudy(const std::string& id,const std::vector<double>& inputs){
    const auto& list=registry();
    const auto it=std::find_if(list.begin(),list.end(),[&](const Entry& e){return e.study.id==id;});
    if(it==list.end())return invalid("Unknown engineering workflow.");
    if(it->study.inputs.size()!=inputs.size())return invalid("Incorrect input count.");
    for(std::size_t i=0;i<inputs.size();++i){
        const auto& input=it->study.inputs[i];
        if(!std::isfinite(inputs[i])||inputs[i]<input.minimum||inputs[i]>input.maximum)
            return invalid("Invalid input: "+input.label+" ("+input.unit+").");
    }
    auto result=it->evaluate(inputs);
    if(!result.valid)return result;
    for(const auto& metric:result.metrics)
        if(!std::isfinite(metric.value))return invalid("Non-finite result; revise inputs.");
    return result;
}
}
