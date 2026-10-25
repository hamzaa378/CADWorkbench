#include "SceneDocument.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace cad { namespace {
constexpr double pi=std::numbers::pi;
TopoDS_Shape cyl(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir d=gp_Dir(0,0,1)){return BRepPrimAPI_MakeCylinder(gp_Ax2(p,d),r,h).Shape();}
TopoDS_Shape fuse(const TopoDS_Shape&a,const TopoDS_Shape&b){BRepAlgoAPI_Fuse op(a,b);op.Build();if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not join precision component geometry.");return op.Shape();}
TopoDS_Shape cut(const TopoDS_Shape&a,const TopoDS_Shape&b){BRepAlgoAPI_Cut op(a,b);op.Build();if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not cut precision component geometry.");return op.Shape();}
TopoDS_Shape annulus(double ro,double ri,double h,double z=0){auto outer=cyl(ro,h,gp_Pnt(0,0,z));return ri>0?cut(outer,cyl(ri,h+0.02,gp_Pnt(0,0,z-0.01))):outer;}
TopoDS_Shape box(double x,double y,double z,double dx,double dy,double dz){return BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),dx,dy,dz).Shape();}
TopoDS_Shape gear(double root,double tip,int teeth,double width,double bore){BRepBuilderAPI_MakePolygon wire;for(int i=0;i<teeth;++i){double a=2*pi*i/teeth,h=pi/(2*teeth);wire.Add(gp_Pnt(root*cos(a-2*h),root*sin(a-2*h),0));wire.Add(gp_Pnt(tip*cos(a-h),tip*sin(a-h),0));wire.Add(gp_Pnt(tip*cos(a+h),tip*sin(a+h),0));wire.Add(gp_Pnt(root*cos(a+2*h),root*sin(a+2*h),0));}wire.Close();if(!wire.IsDone())throw std::runtime_error("Gear profile is invalid.");auto shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,0,width)).Shape();return bore>0?cut(shape,cyl(bore,width+0.02,gp_Pnt(0,0,-0.01))):shape;}
TopoDS_Shape rotateZ(const TopoDS_Shape&shape,double angle){gp_Trsf t;t.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),angle);return BRepBuilderAPI_Transform(shape,t,true).Shape();}
bool wholeIn(double v,int lo,int hi){return std::isfinite(v)&&v>=lo&&v<=hi&&std::floor(v)==v;}
double nacaHalfThickness(double chordFraction,double maximumDepth){
    const double x=std::clamp(chordFraction,0.0,1.0);
    const double profile=0.2969*std::sqrt(x)-0.1260*x-0.3516*x*x+0.2843*x*x*x-0.1015*x*x*x*x;
    return maximumDepth*profile/0.2;
}
TopoDS_Shape wingRib(double chord,double maximumDepth,double plateThickness,double sparRadius,double lighteningRadius){
    constexpr int samples=48;
    const double z=-plateThickness/2.0;
    BRepBuilderAPI_MakePolygon outline;
    for(int i=samples;i>=0;--i){
        const double x=static_cast<double>(i)/samples;
        outline.Add(gp_Pnt(chord*x,nacaHalfThickness(x,maximumDepth),z));
    }
    for(int i=1;i<=samples;++i){
        const double x=static_cast<double>(i)/samples;
        outline.Add(gp_Pnt(chord*x,-nacaHalfThickness(x,maximumDepth),z));
    }
    outline.Close();
    if(!outline.IsDone())throw std::runtime_error("Wing rib airfoil outline is invalid.");
    const auto face=BRepBuilderAPI_MakeFace(outline.Wire()).Face();
    auto shape=BRepPrimAPI_MakePrism(face,gp_Vec(0,0,plateThickness)).Shape();
    const auto throughHole=[&](double fraction,double radius){
        shape=cut(shape,cyl(radius,plateThickness+2.0,gp_Pnt(chord*fraction,0,z-1.0)));
    };
    throughHole(0.30,sparRadius);
    throughHole(0.55,lighteningRadius);
    throughHole(0.76,lighteningRadius);
    return shape;
}
} // namespace
EngineeringPartSpec productionPartSpec(ProductionPart kind){static const QVector<EngineeringPartSpec> specs={
        {"Wing rib",{"Chord length (mm)","Maximum airfoil depth (mm)","Rib thickness (mm)","Spar opening radius (mm)","Lightening hole radius (mm)"},{160.0,26.0,4.0,4.0,3.0},10},
        {"Wing spar root fitting",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{40.3,26.9,87.4,53.8,9.0},2},
        {"Flap hinge bracket",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{104.2,84.3,52.1,12.4,5.0},3},
        {"Landing gear trunnion",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24.5,16.3,111.5,13.6,57.1},4},
        {"Engine mount ring",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{36.0,12.0,64.0,30.0,12.0},5},
        {"Turbine blade root",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{29.1,35.8,15.7,24,6.7},6},
        {"Aircraft seat track fitting",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{52.1,14.9,14.9,8,9.9},7},
        {"Pitot tube housing",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{141.4,106.1,10.9,5.4,49.0},8},
        {"Fuel line bulkhead fitting",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{112.0,72.0,8.0,2.0,12},9},
        {"Avionics rack rail",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.0,80.6,15.7,2.8,15.7},1},
        {"Rail fastening clip",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44.6,29.8,96.7,59.5,9.9},2},
        {"Rail joint fishplate",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{114.2,92.5,57.1,13.6,5.4},3},
        {"Track gauge spacer",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18.0,12.0,82.0,10.0,42.0},4},
        {"Bogie axlebox cover",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{40.3,13.4,71.7,33.6,13.4},5},
        {"Brake disc carrier",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{32.2,39.7,17.4,40,7.4},6},
        {"Coupler knuckle pin",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{57.1,16.3,16.3,8,10.9},7},
        {"Pantograph insulator mount",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{104.0,78.0,8.0,4.0,36.0},8},
        {"Signal lamp housing",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{125.4,80.6,9.0,2.2,14},9},
        {"Overhead wire steady arm",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.9,89.3,17.4,3.1,17.4},1},
        {"Tram door roller",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{49.0,32.6,106.1,65.3,10.9},2},
        {"Excavator bucket tooth adapter",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{84.0,68.0,42.0,10.0,4.0},3},
        {"Track roller carrier",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20.2,13.4,91.8,11.2,47.0},4},
        {"Idler wheel hub",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{44.6,14.9,79.4,37.2,14.9},5},
        {"Hydraulic breaker retainer",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{35.4,43.5,19.0,36,8.2},6},
        {"Conveyor impact bed bracket",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{42.0,12.0,12.0,8,8.0},7},
        {"Drill rod coupling",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{116.5,87.4,9.0,4.5,40.3},8},
        {"Mine ventilation duct flange",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{138.9,89.3,9.9,2.5,16},9},
        {"Rock crusher jaw plate",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{10.9,97.9,19.0,3.4,19.0},1},
        {"Screening deck clamp",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{36.0,24.0,78.0,48.0,8.0},2},
        {"Loader lift cylinder eye",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{94.1,76.2,47.0,11.2,4.5},3},
        {"Bottle capping chuck",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22.3,14.9,101.7,12.4,52.1},4},
        {"Filler nozzle body",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{49.0,16.3,87.0,40.8,16.3},5},
        {"Conveyor guide rail",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{26.0,32.0,14.0,32,6.0},6},
        {"Carton erector cam follower",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{47.0,13.4,13.4,8,9.0},7},
        {"Labeler roller hub",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{129.0,96.7,9.9,5.0,44.6},8},
        {"Rotary table star wheel",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{152.3,97.9,10.9,2.7,18},9},
        {"Mixer paddle hub",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{8.0,72.0,14.0,2.5,14.0},1},
        {"Sanitary pipe clamp",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{40.3,26.9,87.4,53.8,9.0},2},
        {"Tank spray ball",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{104.2,84.3,52.1,12.4,5.0},3},
        {"Food pump rotor",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24.5,16.3,111.5,13.6,57.1},4},
        {"Router collet nut",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{36.0,12.0,64.0,30.0,12.0},5},
        {"Saw arbor flange",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{29.1,35.8,15.7,28,6.7},6},
        {"Bandsaw blade guide",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{52.1,14.9,14.9,8,9.9},7},
        {"Planer cutter head",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{141.4,106.1,10.9,5.4,49.0},8},
        {"Dust hood port",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{112.0,72.0,8.0,2.0,12},9},
        {"Clamp screw handle",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.0,80.6,15.7,2.8,15.7},1},
        {"Weld fixture locator",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44.6,29.8,96.7,59.5,9.9},2},
        {"Welding torch holder",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{114.2,92.5,57.1,13.6,5.4},3},
        {"Sheet metal brake finger",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18.0,12.0,82.0,10.0,42.0},4},
        {"Plasma torch standoff",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{40.3,13.4,71.7,33.6,13.4},5},
        {"Wind tower flange",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{32.2,39.7,17.4,24,7.4},6},
        {"Nacelle yaw bearing ring",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{57.1,16.3,16.3,8,10.9},7},
        {"Solar panel mid clamp",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{104.0,78.0,8.0,4.0,36.0},8},
        {"Cable cleat",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{125.4,80.6,9.0,2.2,14},9},
        {"Insulator cap",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.9,89.3,17.4,3.1,17.4},1},
        {"Switchgear bus duct elbow",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{49.0,32.6,106.1,65.3,10.9},2},
        {"Transformer bushing turret",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{84.0,68.0,42.0,10.0,4.0},3},
        {"Battery rack crossbar",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20.2,13.4,91.8,11.2,47.0},4},
        {"EV charging connector shell",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{44.6,14.9,79.4,37.2,14.9},5},
        {"Substation grounding bar",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{35.4,43.5,19.0,40,8.2},6},
        {"Bicycle pedal spindle",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{42.0,12.0,12.0,8,8.0},7},
        {"Bicycle headset cup",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{116.5,87.4,9.0,4.5,40.3},8},
        {"Wheel hub shell",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{138.9,89.3,9.9,2.5,16},9},
        {"Ski binding plate",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{10.9,97.9,19.0,3.4,19.0},1},
        {"Skateboard truck hanger",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{36.0,24.0,78.0,48.0,8.0},2},
        {"Treadmill roller shaft",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{94.1,76.2,47.0,11.2,4.5},3},
        {"Rowing machine pulley",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22.3,14.9,101.7,12.4,52.1},4},
        {"Weight stack guide block",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{49.0,16.3,87.0,40.8,16.3},5},
        {"Exercise cable swivel",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{26.0,32.0,14.0,36,6.0},6},
        {"Climbing anchor plate",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{47.0,13.4,13.4,8,9.0},7},
        {"Hydrant valve bonnet",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{129.0,96.7,9.9,5.0,44.6},8},
        {"Fire sprinkler escutcheon",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{152.3,97.9,10.9,2.7,18},9},
        {"Water pump coupling",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{8.0,72.0,14.0,2.5,14.0},1},
        {"Meter mounting bracket",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{40.3,26.9,87.4,53.8,9.0},2},
        {"Sewer cleanout cap",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{104.2,84.3,52.1,12.4,5.0},3},
        {"Pipe repair saddle",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24.5,16.3,111.5,13.6,57.1},4},
        {"Check valve disc",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{36.0,12.0,64.0,30.0,12.0},5},
        {"Backflow preventer body",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{29.1,35.8,15.7,32,6.7},6},
        {"Irrigation filter bowl",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{52.1,14.9,14.9,8,9.9},7},
        {"Drain grate frame",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{141.4,106.1,10.9,5.4,49.0},8},
        {"Spinning spindle sleeve",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{112.0,72.0,8.0,2.0,12},9},
        {"Loom shuttle guide",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.0,80.6,15.7,2.8,15.7},1},
        {"Textile roll core",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44.6,29.8,96.7,59.5,9.9},2},
        {"Yarn tension pulley",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{114.2,92.5,57.1,13.6,5.4},3},
        {"Fabric cutter carriage",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18.0,12.0,82.0,10.0,42.0},4},
        {"Printer platen roller",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{40.3,13.4,71.7,33.6,13.4},5},
        {"Paper feed gear",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{32.2,39.7,17.4,28,7.4},6},
        {"Ink cartridge carriage",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{57.1,16.3,16.3,8,10.9},7},
        {"Label applicator bracket",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{104.0,78.0,8.0,4.0,36.0},8},
        {"Binding press platen",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{125.4,80.6,9.0,2.2,14},9},
        {"PLC mounting rail",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.9,89.3,17.4,3.1,17.4},1},
        {"Robot cable gland",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{49.0,32.6,106.1,65.3,10.9},2},
        {"Safety light curtain bracket",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{84.0,68.0,42.0,10.0,4.0},3},
        {"Linear encoder scale mount",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20.2,13.4,91.8,11.2,47.0},4},
        {"Pneumatic gripper finger",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{44.6,14.9,79.4,37.2,14.9},5},
        {"Vacuum ejector body",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{35.4,43.5,19.0,24,8.2},6},
        {"Machine guard hinge",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{42.0,12.0,12.0,8,8.0},7},
        {"Limit switch bracket",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{116.5,87.4,9.0,4.5,40.3},8},
        {"Conveyor photoeye mount",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{138.9,89.3,9.9,2.5,16},9},
        {"Palletizer fork plate",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{10.9,97.9,19.0,3.4,19.0},1}
    };const int i=static_cast<int>(kind);return i>=0&&i<specs.size()?specs[i]:EngineeringPartSpec{};}
bool SceneDocument::addProductionPart(ProductionPart kind,const QVector<double>&d,QString*error){
    const auto spec=productionPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=5){setError(error,"Unknown production component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Production component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString&m){setError(error,m);return false;};
    if(kind==ProductionPart::Wingrib){
        const double sparHalfThickness=nacaHalfThickness(0.30,d[1]);
        const double firstLighteningHalfThickness=nacaHalfThickness(0.55,d[1]);
        const double secondLighteningHalfThickness=nacaHalfThickness(0.76,d[1]);
        if(d[1]>=d[0]*0.4||d[2]>=std::min(d[1]/2.0,d[0]/10.0)||
           d[3]>=sparHalfThickness*0.8||d[4]>=std::min(firstLighteningHalfThickness,secondLighteningHalfThickness)*0.8||
           d[3]+d[4]>=d[0]*0.25||2.0*d[4]>=d[0]*0.21)
            return fail("Airfoil depth, plate thickness, or through-hole sizes do not fit the rib profile.");
    }
    if(spec.geometryFamily==1&&(d[2]<=d[0]||d[3]>=d[0]||d[4]>=d[1]))return fail("Shoulder, bore and shaft lengths do not fit.");
    if(spec.geometryFamily==2&&(d[1]>=d[0]||d[3]<=d[0]||d[4]>=d[2]))return fail("Bore, flange and body dimensions do not fit.");
    if(spec.geometryFamily==3&&(d[3]>=std::min(d[1],d[2])/2||d[4]>=std::min(d[0],d[1])/4))return fail("Port and mounting holes do not fit the block.");
    if(spec.geometryFamily==4&&(d[1]>=d[0]||d[3]>=d[0]||d[4]<=d[0]))return fail("Pipe bore and branch dimensions do not fit.");
    if(spec.geometryFamily==5&&(d[1]>=d[0]||d[3]<=d[1]||d[3]>=d[0]*1.5))return fail("Shaft and wheel hub dimensions do not fit.");
    if(spec.geometryFamily==6&&(!wholeIn(d[3],12,60)||d[0]>=d[1]||d[4]>=d[0]))return fail("Gear tooth count or radii do not fit.");
    if(spec.geometryFamily==7&&(!wholeIn(d[3],4,16)||d[1]>=d[0]*0.6||d[4]>=d[0]-d[1]))return fail("Blade count or dimensions do not fit.");
    if(spec.geometryFamily==8&&(d[3]*2>=std::min(d[0],d[1])||d[4]+2*d[3]>=std::min(d[0],d[1])))return fail("Plate holes or spacing do not fit.");
    if(spec.geometryFamily==9&&(!wholeIn(d[4],4,24)||d[3]*d[4]>=d[0]-2))return fail("Fin count or thickness does not fit the base.");
    try{TopoDS_Shape shape;switch(spec.geometryFamily){
      case 10:shape=wingRib(d[0],d[1],d[2],d[3],d[4]);break;
      case 1:{auto shaft=cyl(d[0],d[1]);shaft=fuse(shaft,cyl(d[2],d[4],gp_Pnt(0,0,d[1]-d[4]*0.9)));shape=cut(shaft,cyl(d[3],d[1]+0.02,gp_Pnt(0,0,-0.01)));break;}
      case 2:{auto body=annulus(d[0],d[1],d[2]);shape=fuse(body,annulus(d[3],d[1],d[4],d[2]-d[4]));break;}
      case 3:{auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);body=cut(body,cyl(d[3],d[0]+2,gp_Pnt(-d[0]/2-1,0,d[2]/2),gp_Dir(1,0,0)));for(double x:{-d[0]*0.32,d[0]*0.32})for(double y:{-d[1]*0.32,d[1]*0.32})body=cut(body,cyl(d[4],d[2]+2,gp_Pnt(x,y,-1)));shape=body;break;}
      case 4:{auto outer=cyl(d[0],d[2],gp_Pnt(0,0,-d[2]/2));outer=fuse(outer,cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0)));auto bores=fuse(cyl(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1)),cyl(d[1]*0.8,d[4]+2,gp_Pnt(0,0,0),gp_Dir(1,0,0)));shape=cut(outer,bores);break;}
      case 5:{auto body=cyl(d[0],d[2]);body=fuse(body,cyl(d[3],d[4],gp_Pnt(0,0,d[2]-d[4]*0.65)));shape=cut(body,cyl(d[1],d[2]+d[4]+2,gp_Pnt(0,0,-1)));break;}
      case 6:shape=gear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
      case 7:{auto body=annulus(d[0],d[0]*0.88,d[2]);body=fuse(body,cyl(d[1],d[2]));auto blade=box(d[1]*0.7,-d[4]/2,d[2]*0.15,d[0]-d[1]*0.7,d[4],d[2]*0.7);for(int i=0;i<static_cast<int>(d[3]);++i)body=fuse(body,rotateZ(blade,2*pi*i/d[3]));shape=body;break;}
      case 8:{auto plate=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);double p=d[4]/2;for(double x:{-p,p})for(double y:{-p,p})plate=cut(plate,cyl(d[3],d[2]+2,gp_Pnt(x,y,-1)));shape=plate;break;}
      case 9:{auto base=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);double gap=(d[0]-d[4]*d[3])/(d[4]+1);for(int i=0;i<static_cast<int>(d[4]);++i){double x=-d[0]/2+gap+(gap+d[3])*i;base=fuse(base,box(x,-d[1]/2,d[2],d[3],d[1],d[2]));}shape=base;break;}
      default:return fail("Production component has an unsupported geometry family.");}
      if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid())return fail("Generated production component failed topology validation.");GProp_GProps props;BRepGProp::VolumeProperties(shape,props);if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated production component has no positive volume.");checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),shape,true,"#6f83b5"});return true;
    }catch(const Standard_Failure&e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}catch(const std::exception&e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
