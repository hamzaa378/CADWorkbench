#include "SceneDocument.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
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
} // namespace
EngineeringPartSpec precisionPartSpec(PrecisionPart kind){static const QVector<EngineeringPartSpec> specs={
        {"Servo mount plate",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{8.0,72.0,14.0,2.5,14.0},1},
        {"Robot wrist flange",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{40.3,26.9,87.4,53.8,9.0},2},
        {"End effector jaw",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{104.2,84.3,52.1,12.4,5.0},3},
        {"Linear slide carriage",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24.5,16.3,111.5,13.6,57.1},4},
        {"Rotary index table",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{36.0,12.0,64.0,30.0,12.0},5},
        {"Cable carrier link",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{29.1,35.8,15.7,24,6.7},6},
        {"Sensor bracket",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{52.1,14.9,14.9,8,9.9},7},
        {"Actuator clevis",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{141.4,106.1,10.9,5.4,49.0},8},
        {"Encoder housing",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{112.0,72.0,8.0,2.0,12},9},
        {"Suction cup manifold",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.0,80.6,15.7,2.8,15.7},1},
        {"Microscope stage",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44.6,29.8,96.7,59.5,9.9},2},
        {"Centrifuge rotor",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{114.2,92.5,57.1,13.6,5.4},3},
        {"Pipette carousel",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18.0,12.0,82.0,10.0,42.0},4},
        {"Lab bottle cap",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{40.3,13.4,71.7,33.6,13.4},5},
        {"Syringe pump carriage",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{32.2,39.7,17.4,40,7.4},6},
        {"Instrument panel plate",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{57.1,16.3,16.3,8,10.9},7},
        {"Sample vial rack",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{104.0,78.0,8.0,4.0,36.0},8},
        {"Medical gas outlet",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{125.4,80.6,9.0,2.2,14},9},
        {"Oxygen regulator body",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.9,89.3,17.4,3.1,17.4},1},
        {"Sterilizer door flange",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{49.0,32.6,106.1,65.3,10.9},2},
        {"Lens cell",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{84.0,68.0,42.0,10.0,4.0},3},
        {"Mirror mount",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20.2,13.4,91.8,11.2,47.0},4},
        {"Telescope focuser tube",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{44.6,14.9,79.4,37.2,14.9},5},
        {"Optical bench post",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{35.4,43.5,19.0,36,8.2},6},
        {"Camera cage plate",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{42.0,12.0,12.0,8,8.0},7},
        {"Filter wheel hub",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{116.5,87.4,9.0,4.5,40.3},8},
        {"Prism holder",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{138.9,89.3,9.9,2.5,16},9},
        {"Laser diode heat sink",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{10.9,97.9,19.0,3.4,19.0},1},
        {"Fiber spool hub",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{36.0,24.0,78.0,48.0,8.0},2},
        {"Lidar sensor housing",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{94.1,76.2,47.0,11.2,4.5},3},
        {"Wind turbine hub",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22.3,14.9,101.7,12.4,52.1},4},
        {"Solar tracker bearing",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{49.0,16.3,87.0,40.8,16.3},5},
        {"Battery module end plate",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{26.0,32.0,14.0,32,6.0},6},
        {"Inverter busbar block",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{47.0,13.4,13.4,8,9.0},7},
        {"Fuel cell manifold",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{129.0,96.7,9.9,5.0,44.6},8},
        {"Electrolyzer end cap",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{152.3,97.9,10.9,2.7,18},9},
        {"Geothermal wellhead flange",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{8.0,72.0,14.0,2.5,14.0},1},
        {"Hydrant nozzle",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{40.3,26.9,87.4,53.8,9.0},2},
        {"Water meter body",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{104.2,84.3,52.1,12.4,5.0},3},
        {"Pressure gauge case",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24.5,16.3,111.5,13.6,57.1},4},
        {"Scaffolding base plate",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{36.0,12.0,64.0,30.0,12.0},5},
        {"Guardrail post shoe",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{29.1,35.8,15.7,28,6.7},6},
        {"Anchor bolt template",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{52.1,14.9,14.9,8,9.9},7},
        {"Bridge bearing plate",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{141.4,106.1,10.9,5.4,49.0},8},
        {"Expansion joint anchor",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{112.0,72.0,8.0,2.0,12},9},
        {"Handrail elbow",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.0,80.6,15.7,2.8,15.7},1},
        {"Pipe rack saddle",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44.6,29.8,96.7,59.5,9.9},2},
        {"Crane hook block",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{114.2,92.5,57.1,13.6,5.4},3},
        {"Lifting lug plate",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18.0,12.0,82.0,10.0,42.0},4},
        {"Structural base shoe",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{40.3,13.4,71.7,33.6,13.4},5},
        {"Trailer axle spindle",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{32.2,39.7,17.4,24,7.4},6},
        {"Wheel spacer",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{57.1,16.3,16.3,8,10.9},7},
        {"Brake master cylinder body",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{104.0,78.0,8.0,4.0,36.0},8},
        {"Steering rack end",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{125.4,80.6,9.0,2.2,14},9},
        {"Shock absorber clevis",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{9.9,89.3,17.4,3.1,17.4},1},
        {"Seat rail bracket",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{49.0,32.6,106.1,65.3,10.9},2},
        {"Wiper motor housing",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{84.0,68.0,42.0,10.0,4.0},3},
        {"Roof rack foot",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20.2,13.4,91.8,11.2,47.0},4},
        {"Bicycle crank spider",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{44.6,14.9,79.4,37.2,14.9},5},
        {"Scooter fork crown",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{35.4,43.5,19.0,40,8.2},6},
        {"Laptop hinge barrel",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{42.0,12.0,12.0,8,8.0},7},
        {"Camera tripod socket",{"Plate width (mm)","Plate depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{116.5,87.4,9.0,4.5,40.3},8},
        {"Speaker basket",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{138.9,89.3,9.9,2.5,16},9},
        {"Headphone yoke",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{10.9,97.9,19.0,3.4,19.0},1},
        {"Game controller shell",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{36.0,24.0,78.0,48.0,8.0},2},
        {"Power tool gearbox cover",{"Block width (mm)","Block depth (mm)","Block height (mm)","Side-bore radius (mm)","Mount-hole radius (mm)"},{94.1,76.2,47.0,11.2,4.5},3},
        {"Battery terminal block",{"Run outer radius (mm)","Run bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22.3,14.9,101.7,12.4,52.1},4},
        {"Appliance motor mount",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","End flange radius (mm)","Flange length (mm)"},{49.0,16.3,87.0,40.8,16.3},5},
        {"Smart thermostat plate",{"Gear root radius (mm)","Gear tip radius (mm)","Gear width (mm)","Tooth count (12-60)","Bore radius (mm)"},{26.0,32.0,14.0,36,6.0},6},
        {"Drone motor mount",{"Frame radius (mm)","Hub radius (mm)","Body thickness (mm)","Blade count (4-16)","Blade width (mm)"},{47.0,13.4,13.4,8,9.0},7}
    };const int i=static_cast<int>(kind);return i>=0&&i<specs.size()?specs[i]:EngineeringPartSpec{};}
bool SceneDocument::addPrecisionPart(PrecisionPart kind,const QVector<double>&d,QString*error){
    const auto spec=precisionPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=5){setError(error,"Unknown precision component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Precision component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString&m){setError(error,m);return false;};
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
      case 1:{auto shaft=cyl(d[0],d[1]);shaft=fuse(shaft,cyl(d[2],d[4],gp_Pnt(0,0,d[1]-d[4]*0.9)));shape=cut(shaft,cyl(d[3],d[1]+0.02,gp_Pnt(0,0,-0.01)));break;}
      case 2:{auto body=annulus(d[0],d[1],d[2]);shape=fuse(body,annulus(d[3],d[1],d[4],d[2]-d[4]));break;}
      case 3:{auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);body=cut(body,cyl(d[3],d[0]+2,gp_Pnt(-d[0]/2-1,0,d[2]/2),gp_Dir(1,0,0)));for(double x:{-d[0]*0.32,d[0]*0.32})for(double y:{-d[1]*0.32,d[1]*0.32})body=cut(body,cyl(d[4],d[2]+2,gp_Pnt(x,y,-1)));shape=body;break;}
      case 4:{auto outer=cyl(d[0],d[2],gp_Pnt(0,0,-d[2]/2));outer=fuse(outer,cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0)));auto bores=fuse(cyl(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1)),cyl(d[1]*0.8,d[4]+2,gp_Pnt(0,0,0),gp_Dir(1,0,0)));shape=cut(outer,bores);break;}
      case 5:{auto body=cyl(d[0],d[2]);body=fuse(body,cyl(d[3],d[4],gp_Pnt(0,0,d[2]-d[4]*0.65)));shape=cut(body,cyl(d[1],d[2]+d[4]+2,gp_Pnt(0,0,-1)));break;}
      case 6:shape=gear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
      case 7:{auto body=annulus(d[0],d[0]*0.88,d[2]);body=fuse(body,cyl(d[1],d[2]));auto blade=box(d[1]*0.7,-d[4]/2,d[2]*0.15,d[0]-d[1]*0.7,d[4],d[2]*0.7);for(int i=0;i<static_cast<int>(d[3]);++i)body=fuse(body,rotateZ(blade,2*pi*i/d[3]));shape=body;break;}
      case 8:{auto plate=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);double p=d[4]/2;for(double x:{-p,p})for(double y:{-p,p})plate=cut(plate,cyl(d[3],d[2]+2,gp_Pnt(x,y,-1)));shape=plate;break;}
      case 9:{auto base=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);double gap=(d[0]-d[4]*d[3])/(d[4]+1);for(int i=0;i<static_cast<int>(d[4]);++i){double x=-d[0]/2+gap+(gap+d[3])*i;base=fuse(base,box(x,-d[1]/2,d[2],d[3],d[1],d[2]));}shape=base;break;}
      default:return fail("Precision component has an unsupported geometry family.");}
      if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid())return fail("Generated precision component failed topology validation.");GProp_GProps props;BRepGProp::VolumeProperties(shape,props);if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated precision component has no positive volume.");checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),shape,true,"#6f83b5"});return true;
    }catch(const Standard_Failure&e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}catch(const std::exception&e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
