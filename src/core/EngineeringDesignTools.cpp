#include "EngineeringDesignTools.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace cad {
namespace {
using Vec = std::vector<double>;
using Formula = double (*)(const Vec&);
struct Entry { EngineeringTool tool; Formula formula; };
constexpr double pi = std::numbers::pi_v<double>;
constexpr double tiny = 1e-12;

auto input(const char* key, const char* label, const char* unit,
           double example, double lo = tiny, double hi = 1e10) -> EngineeringInput {
    return {key, label, unit, example, lo, hi};
}

const std::vector<Entry>& registry() {
    static const std::vector<Entry> entries = [] {
        std::vector<Entry> all;
        auto add = [&](const char* group, const char* id, const char* title,
                       const char* unit, const char* assumptions,
                       std::initializer_list<EngineeringInput> inputs, Formula fn) {
            all.push_back({{id,group,title,unit,assumptions,inputs},fn});
        };
        // Geometry, section properties and mass (10).
        add("Sections & Mass","rectangle_area","Rectangular area","mm^2","Nominal, flat rectangle.",
            {input("width_mm","Width","mm",50),input("height_mm","Height","mm",20)},
            +[](const Vec& v){return v[0]*v[1];});
        add("Sections & Mass","circle_area","Circular area","mm^2","Nominal full circle.",
            {input("diameter_mm","Diameter","mm",20)},
            +[](const Vec& v){return pi*v[0]*v[0]/4;});
        add("Sections & Mass","annulus_area","Annular area","mm^2","Outer diameter must exceed inner diameter.",
            {input("outer_d_mm","Outer diameter","mm",30),input("inner_d_mm","Inner diameter","mm",20)},
            +[](const Vec& v){return v[0]>v[1]?pi*(v[0]*v[0]-v[1]*v[1])/4:std::numeric_limits<double>::quiet_NaN();});
        add("Sections & Mass","rectangle_inertia","Rectangle section second moment","mm^4","Centroidal bending axis parallel to width.",
            {input("width_mm","Section width","mm",40),input("height_mm","Section height","mm",60)},
            +[](const Vec& v){return v[0]*std::pow(v[1],3)/12;});
        add("Sections & Mass","circle_inertia","Circular section second moment","mm^4","Centroidal diametral axis.",
            {input("diameter_mm","Diameter","mm",40)},
            +[](const Vec& v){return pi*std::pow(v[0],4)/64;});
        add("Sections & Mass","annulus_inertia","Tube section second moment","mm^4","Centroidal diametral axis; outer > inner diameter.",
            {input("outer_d_mm","Outer diameter","mm",40),input("inner_d_mm","Inner diameter","mm",20)},
            +[](const Vec& v){return v[0]>v[1]?pi*(std::pow(v[0],4)-std::pow(v[1],4))/64:std::numeric_limits<double>::quiet_NaN();});
        add("Sections & Mass","cylinder_volume","Solid cylinder volume","mm^3","Cylindrical solid, nominal dimensions.",
            {input("diameter_mm","Diameter","mm",20),input("length_mm","Length","mm",100)},
            +[](const Vec& v){return pi*v[0]*v[0]*v[1]/4;});
        add("Sections & Mass","tube_volume","Hollow cylinder volume","mm^3","Constant wall, outer > inner diameter.",
            {input("outer_d_mm","Outer diameter","mm",40),input("inner_d_mm","Inner diameter","mm",20),input("length_mm","Length","mm",100)},
            +[](const Vec& v){return v[0]>v[1]?pi*(v[0]*v[0]-v[1]*v[1])*v[2]/4:std::numeric_limits<double>::quiet_NaN();});
        add("Sections & Mass","plate_mass","Rectangular plate mass","kg","Homogeneous material; density in kg/m^3.",
            {input("width_mm","Width","mm",100),input("height_mm","Height","mm",200),input("thickness_mm","Thickness","mm",5),input("density_kgm3","Density","kg/m^3",7850)},
            +[](const Vec& v){return v[0]*v[1]*v[2]*v[3]*1e-9;});
        add("Sections & Mass","rod_mass","Solid round rod mass","kg","Homogeneous material; density in kg/m^3.",
            {input("diameter_mm","Diameter","mm",20),input("length_mm","Length","mm",1000),input("density_kgm3","Density","kg/m^3",7850)},
            +[](const Vec& v){return pi*v[0]*v[0]*v[1]*v[2]*1e-9/4;});
        // Nominal stresses and loading (10).
        add("Stress & Strength","axial_stress","Axial normal stress","MPa","Uniform axial load, no stress concentration.",
            {input("load_N","Axial load","N",10000),input("area_mm2","Net section area","mm^2",100)},
            +[](const Vec& v){return v[0]/v[1];});
        add("Stress & Strength","axial_strain","Elastic axial strain","1","Linear elastic Hooke law; E supplied in GPa.",
            {input("stress_MPa","Stress","MPa",100),input("E_GPa","Young's modulus","GPa",200)},
            +[](const Vec& v){return v[0]/(1000*v[1]);});
        add("Stress & Strength","axial_elongation","Elastic bar elongation","mm","Uniform prismatic bar, axial load, linear elasticity.",
            {input("load_N","Axial load","N",10000),input("length_mm","Length","mm",1000),input("area_mm2","Area","mm^2",100),input("E_GPa","Young's modulus","GPa",200)},
            +[](const Vec& v){return v[0]*v[1]/(v[2]*v[3]*1000);});
        add("Stress & Strength","bending_stress","Elastic bending stress","MPa","Straight beam, sigma = M*c/I, no concentration.",
            {input("moment_Nmm","Bending moment","N·mm",100000),input("extreme_fiber_mm","Extreme-fiber distance","mm",20),input("I_mm4","Second moment","mm^4",100000)},
            +[](const Vec& v){return v[0]*v[1]/v[2];});
        add("Stress & Strength","solid_shaft_shear","Solid shaft torsional shear","MPa","Circular solid shaft, Saint-Venant torsion.",
            {input("torque_Nmm","Torque","N·mm",100000),input("diameter_mm","Diameter","mm",20)},
            +[](const Vec& v){return 16*v[0]/(pi*std::pow(v[1],3));});
        add("Stress & Strength","tube_shaft_shear","Hollow shaft torsional shear","MPa","Circular hollow shaft; outer > inner diameter.",
            {input("torque_Nmm","Torque","N·mm",100000),input("outer_d_mm","Outer diameter","mm",30),input("inner_d_mm","Inner diameter","mm",20)},
            +[](const Vec& v){return v[1]>v[2]?16*v[0]*v[1]/(pi*(std::pow(v[1],4)-std::pow(v[2],4))):std::numeric_limits<double>::quiet_NaN();});
        add("Stress & Strength","cylinder_hoop_stress","Thin cylinder hoop stress","MPa","Thin-wall membrane estimate; t << diameter; pressure in MPa.",
            {input("pressure_MPa","Internal pressure","MPa",1),input("inner_d_mm","Internal diameter","mm",100),input("thickness_mm","Wall thickness","mm",5)},
            +[](const Vec& v){return v[0]*v[1]/(2*v[2]);});
        add("Stress & Strength","cylinder_axial_stress","Thin cylinder axial stress","MPa","Closed-end thin-wall membrane estimate; t << diameter.",
            {input("pressure_MPa","Internal pressure","MPa",1),input("inner_d_mm","Internal diameter","mm",100),input("thickness_mm","Wall thickness","mm",5)},
            +[](const Vec& v){return v[0]*v[1]/(4*v[2]);});
        add("Stress & Strength","pin_bearing_stress","Pin bearing pressure","MPa","Average projected bearing area d*t.",
            {input("load_N","Load","N",10000),input("diameter_mm","Pin diameter","mm",10),input("thickness_mm","Plate thickness","mm",5)},
            +[](const Vec& v){return v[0]/(v[1]*v[2]);});
        add("Stress & Strength","yield_safety_factor","Nominal yield safety factor","1","Yield stress / equivalent working stress; no code factors.",
            {input("yield_MPa","Yield strength","MPa",250),input("stress_MPa","Equivalent stress","MPa",100)},
            +[](const Vec& v){return v[0]/v[1];});
        // Standard simple beams (10). Beam theory is Euler-Bernoulli.
        add("Beams & Columns","simply_center_deflection","Simply supported, center point deflection","mm","Elastic small-deflection, central point load; constant EI.",
            {input("load_N","Point load","N",1000),input("length_mm","Span","mm",1000),input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Second moment","mm^4",1e6)},
            +[](const Vec& v){return v[0]*std::pow(v[1],3)/(48*1000*v[2]*v[3]);});
        add("Beams & Columns","simply_udl_deflection","Simply supported, uniform load deflection","mm","Elastic small-deflection; w in N/mm (NOT N/m).",
            {input("udl_N_per_mm","Distributed load","N/mm",1),input("length_mm","Span","mm",1000),input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Second moment","mm^4",1e6)},
            +[](const Vec& v){return 5*v[0]*std::pow(v[1],4)/(384*1000*v[2]*v[3]);});
        add("Beams & Columns","cantilever_tip_deflection","Cantilever, tip load deflection","mm","Elastic small-deflection with ideal fixed support.",
            {input("load_N","Tip load","N",1000),input("length_mm","Length","mm",1000),input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Second moment","mm^4",1e6)},
            +[](const Vec& v){return v[0]*std::pow(v[1],3)/(3*1000*v[2]*v[3]);});
        add("Beams & Columns","cantilever_udl_deflection","Cantilever, uniform load deflection","mm","Elastic small-deflection; w in N/mm.",
            {input("udl_N_per_mm","Distributed load","N/mm",1),input("length_mm","Length","mm",1000),input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Second moment","mm^4",1e6)},
            +[](const Vec& v){return v[0]*std::pow(v[1],4)/(8*1000*v[2]*v[3]);});
        add("Beams & Columns","simply_center_moment","Simply supported, center load maximum moment","N·mm","One central transverse point load, two simple supports.",
            {input("load_N","Point load","N",1000),input("length_mm","Span","mm",1000)},
            +[](const Vec& v){return v[0]*v[1]/4;});
        add("Beams & Columns","simply_udl_moment","Simply supported, UDL maximum moment","N·mm","Uniform load over entire span; w in N/mm.",
            {input("udl_N_per_mm","Distributed load","N/mm",1),input("length_mm","Span","mm",1000)},
            +[](const Vec& v){return v[0]*v[1]*v[1]/8;});
        add("Beams & Columns","cantilever_end_moment","Cantilever, tip load root moment","N·mm","One transverse load at free end, ideal fixed support.",
            {input("load_N","Tip load","N",1000),input("length_mm","Length","mm",1000)},
            +[](const Vec& v){return v[0]*v[1];});
        add("Beams & Columns","cantilever_tip_slope","Cantilever, tip load slope","rad","Small-angle, elastic Euler-Bernoulli theory.",
            {input("load_N","Tip load","N",1000),input("length_mm","Length","mm",1000),input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Second moment","mm^4",1e6)},
            +[](const Vec& v){return v[0]*v[1]*v[1]/(2*1000*v[2]*v[3]);});
        add("Beams & Columns","euler_column_load","Euler elastic buckling load","N","Slender elastic column; effective length factor K; not a design capacity.",
            {input("E_GPa","Young's modulus","GPa",200),input("I_mm4","Least second moment","mm^4",1e6),input("length_mm","Clear length","mm",1000),input("factor_K","Effective-length factor K","1",1,0.1,10)},
            +[](const Vec& v){return pi*pi*1000*v[0]*v[1]/std::pow(v[2]*v[3],2);});
        add("Beams & Columns","rect_beam_max_shear","Rectangle beam maximum transverse shear","MPa","1.5 V/A; homogeneous rectangular section, elastic theory.",
            {input("load_N","Shear force","N",1000),input("width_mm","Width","mm",20),input("height_mm","Height","mm",40)},
            +[](const Vec& v){return 1.5*v[0]/(v[1]*v[2]);});
        // Fastening, joining and fit (10).
        add("Fasteners & Fits","iso_bolt_tensile_area","Metric thread tensile stress area","mm^2","Approx. ISO basic thread: A_t = pi/4*(d - 0.9382*p)^2.",
            {input("nominal_d_mm","Nominal thread diameter","mm",10),input("pitch_mm","Thread pitch","mm",1.5)},
            +[](const Vec& v){return v[0]>0.9382*v[1]?pi*std::pow(v[0]-0.9382*v[1],2)/4:std::numeric_limits<double>::quiet_NaN();});
        add("Fasteners & Fits","bolt_tensile_stress","Bolt nominal tensile stress","MPa","Gross axial force divided by bolt tensile stress area.",
            {input("load_N","Bolt axial load","N",10000),input("area_mm2","Tensile stress area","mm^2",58)},
            +[](const Vec& v){return v[0]/v[1];});
        add("Fasteners & Fits","bolt_torque_estimate","Torque-preload estimate","N·m","Empirical T=K*F*d; K depends strongly on friction.",
            {input("nut_factor","Nut factor K","1",0.2,0.05,0.5),input("preload_N","Target preload","N",10000),input("nominal_d_mm","Bolt diameter","mm",10)},
            +[](const Vec& v){return v[0]*v[1]*v[2]/1000;});
        add("Fasteners & Fits","bolt_group_load","Direct shear per bolt","N","Concentric shear shared equally; excludes moment and prying.",
            {input("load_N","Total transverse load","N",10000),input("count","Number of bolts","count",4,1,10000)},
            +[](const Vec& v){return v[0]/v[1];});
        add("Fasteners & Fits","bolt_group_shear_stress","Bolt group direct shear stress","MPa","Equal sharing across bolts and one shear plane each.",
            {input("load_N","Total transverse load","N",10000),input("count","Number of bolts","count",4,1,10000),input("area_mm2","Area per bolt","mm^2",78.5)},
            +[](const Vec& v){return v[0]/(v[1]*v[2]);});
        add("Fasteners & Fits","fillet_weld_throat","Equal-leg fillet weld throat","mm","Theoretical effective throat 0.707*leg; not a code capacity.",
            {input("leg_mm","Weld leg","mm",6)},
            +[](const Vec& v){return v[0]/std::sqrt(2.0);});
        add("Fasteners & Fits","fillet_weld_nominal_shear","Fillet weld nominal shear stress","MPa","Single effective weld length, uniform load, no eccentricity.",
            {input("load_N","Load","N",10000),input("leg_mm","Equal weld leg","mm",6),input("weld_length_mm","Effective weld length","mm",100)},
            +[](const Vec& v){return v[0]*std::sqrt(2.0)/(v[1]*v[2]);});
        add("Fasteners & Fits","minimum_clearance","Minimum diametral fit clearance","mm","Hole minimum minus shaft maximum; negative means interference.",
            {input("hole_min_mm","Minimum hole","mm",20.01),input("shaft_max_mm","Maximum shaft","mm",20,0,1e10)},
            +[](const Vec& v){return v[0]-v[1];});
        add("Fasteners & Fits","maximum_clearance","Maximum diametral fit clearance","mm","Hole maximum minus shaft minimum.",
            {input("hole_max_mm","Maximum hole","mm",20.03),input("shaft_min_mm","Minimum shaft","mm",19.98)},
            +[](const Vec& v){return v[0]-v[1];});
        add("Fasteners & Fits","diametral_interference","Nominal press-fit interference","mm","Shaft diameter minus hole diameter; positive indicates interference.",
            {input("shaft_d_mm","Shaft diameter","mm",20.02),input("hole_d_mm","Hole diameter","mm",20)},
            +[](const Vec& v){return v[0]-v[1];});
        // Thermal, tolerance and machining estimates (10).
        add("Production & Thermal","thermal_expansion","Free linear thermal expansion","mm","Uniform expansion; alpha in microstrain/K; signed temperature change.",
            {input("alpha_microK","Expansion coefficient","10^-6/K",12,0,1e5),input("length_mm","Original length","mm",1000),input("delta_temperature_K","Temperature change","K",50,-1e5,1e5)},
            +[](const Vec& v){return v[0]*1e-6*v[1]*v[2];});
        add("Production & Thermal","thermal_stress_magnitude","Fully restrained thermal stress magnitude","MPa","Uniaxial linear elasticity; ideal full restraint and no yielding.",
            {input("E_GPa","Young's modulus","GPa",200),input("alpha_microK","Expansion coefficient","10^-6/K",12,0,1e5),input("delta_temperature_K","Temperature change","K",50,-1e5,1e5)},
            +[](const Vec& v){return 1000*v[0]*v[1]*1e-6*std::abs(v[2]);});
        add("Production & Thermal","tolerance_worst_case","Three-part worst-case tolerance","mm","Sum of nonnegative absolute dimensional tolerances.",
            {input("tol1_mm","Tolerance 1","mm",0.1,0),input("tol2_mm","Tolerance 2","mm",0.1,0),input("tol3_mm","Tolerance 3","mm",0.1,0)},
            +[](const Vec& v){return v[0]+v[1]+v[2];});
        add("Production & Thermal","tolerance_rss","Three-part statistical RSS tolerance","mm","Independent zero-centered contributors, same statistical definition.",
            {input("tol1_mm","Tolerance 1","mm",0.1,0),input("tol2_mm","Tolerance 2","mm",0.1,0),input("tol3_mm","Tolerance 3","mm",0.1,0)},
            +[](const Vec& v){return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);});
        add("Production & Thermal","sheet_bend_allowance","Sheet-metal bend allowance","mm","Neutral-axis estimate: theta(rad)*(inside radius + K*thickness).",
            {input("angle_deg","Bend angle","deg",90,1e-6,179),input("radius_mm","Inside radius","mm",2),input("thickness_mm","Thickness","mm",1),input("k_factor","K factor","1",0.33,0,1)},
            +[](const Vec& v){return v[0]*pi/180*(v[1]+v[3]*v[2]);});
        add("Production & Thermal","sheet_bend_deduction","Sheet-metal bend deduction","mm","BD=2*(r+t)*tan(theta/2)-BA; theta is bend angle.",
            {input("angle_deg","Bend angle","deg",90,1e-6,170),input("radius_mm","Inside radius","mm",2),input("thickness_mm","Thickness","mm",1),input("k_factor","K factor","1",0.33,0,1)},
            +[](const Vec& v){const double a=v[0]*pi/180;return 2*(v[1]+v[2])*std::tan(a/2)-a*(v[1]+v[3]*v[2]);});
        add("Production & Thermal","drilling_time","Drilling feed time","min","Depth / (feed per revolution * spindle speed); no peck/retract allowance.",
            {input("depth_mm","Hole depth","mm",30),input("feed_mm_rev","Feed per revolution","mm/rev",0.1),input("rpm","Spindle speed","rev/min",1000)},
            +[](const Vec& v){return v[0]/(v[1]*v[2]);});
        add("Production & Thermal","turning_time","Turning feed time","min","Axial pass length / (feed per revolution * spindle speed).",
            {input("length_mm","Cut length","mm",100),input("feed_mm_rev","Feed per revolution","mm/rev",0.2),input("rpm","Spindle speed","rev/min",800)},
            +[](const Vec& v){return v[0]/(v[1]*v[2]);});
        add("Production & Thermal","milling_time","Milling feed time","min","Straight path length / programmed feed; no approach/retract.",
            {input("length_mm","Toolpath length","mm",200),input("feed_mm_min","Feed rate","mm/min",300)},
            +[](const Vec& v){return v[0]/v[1];});
        add("Production & Thermal","spindle_speed","Cutting-speed spindle RPM","rev/min","Nominal surface speed conversion for circular tool/workpiece.",
            {input("cut_speed_m_min","Cutting speed","m/min",100),input("diameter_mm","Tool or work diameter","mm",20)},
            +[](const Vec& v){return 1000*v[0]/(pi*v[1]);});
        return all;
    }();
    return entries;
}
}

const std::vector<EngineeringTool>& engineeringToolCatalog() {
    static const std::vector<EngineeringTool> result = [] {
        std::vector<EngineeringTool> out;
        for (const auto& entry : registry()) out.push_back(entry.tool);
        return out;
    }();
    return result;
}

EngineeringResult evaluateEngineeringTool(const std::string& id, const std::vector<double>& values) {
    const auto& entries = registry();
    const auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) {
        return entry.tool.id == id;
    });
    if (it == entries.end()) return {false,0.0,"Unknown engineering calculation."};
    if (values.size() != it->tool.inputs.size()) return {false,0.0,"Input count does not match the tool definition."};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto& spec = it->tool.inputs[i];
        if (!std::isfinite(values[i]) || values[i] < spec.minimum || values[i] > spec.maximum)
            return {false,0.0,"Invalid value for " + spec.label + " (" + spec.unit + ")."};
        if (spec.key == "count" && std::floor(values[i]) != values[i])
            return {false,0.0,"Bolt count must be an integer."};
    }
    // Thin-wall equations are misleading outside their membrane assumptions.
    if ((id == "cylinder_hoop_stress" || id == "cylinder_axial_stress") &&
        values[2] > values[1] / 10.0)
        return {false,0.0,"Thin-wall estimate requires thickness <= 10% of inner diameter."};
    const double answer = it->formula(values);
    if (!std::isfinite(answer)) return {false,0.0,"Invalid geometry or unsuitable parameter combination."};
    return {true,answer,{}};
}
} // namespace cad
