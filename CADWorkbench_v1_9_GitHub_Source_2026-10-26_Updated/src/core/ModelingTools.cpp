#include "SceneDocument.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <NCollection_HArray1.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Trsf.hxx>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cad { namespace {
constexpr double pi=std::numbers::pi;
TopoDS_Shape box(double x,double y,double z,double dx,double dy,double dz){return BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),dx,dy,dz).Shape();}
TopoDS_Shape cylinder(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir axis=gp_Dir(0,0,1)){return BRepPrimAPI_MakeCylinder(gp_Ax2(p,axis),r,h).Shape();}
TopoDS_Shape fuse(const TopoDS_Shape&a,const TopoDS_Shape&b){BRepAlgoAPI_Fuse op(a,b);op.Build();if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not fuse modeling-tool geometry.");return op.Shape();}
TopoDS_Shape cut(const TopoDS_Shape&a,const TopoDS_Shape&b){BRepAlgoAPI_Cut op(a,b);op.Build();if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not cut modeling-tool geometry.");return op.Shape();}
TopoDS_Shape prism(const QVector<gp_Pnt>&points,double length){BRepBuilderAPI_MakePolygon wire;for(const auto&p:points)wire.Add(p);wire.Close();if(!wire.IsDone())throw std::runtime_error("Modeling-tool profile is invalid.");return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,0,length)).Shape();}
TopoDS_Shape radialGear(double root,double tip,int teeth,double width,double bore){
    BRepBuilderAPI_MakePolygon wire;
    for(int i=0;i<teeth;++i){const double a=2*pi*i/teeth,h=pi/(2*teeth);wire.Add(gp_Pnt(root*std::cos(a-2*h),root*std::sin(a-2*h),0));wire.Add(gp_Pnt(tip*std::cos(a-h),tip*std::sin(a-h),0));wire.Add(gp_Pnt(tip*std::cos(a+h),tip*std::sin(a+h),0));wire.Add(gp_Pnt(root*std::cos(a+2*h),root*std::sin(a+2*h),0));}
    wire.Close();auto shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,0,width)).Shape();
    return bore>0?cut(shape,cylinder(bore,width+2,gp_Pnt(0,0,-1))):shape;
}
TopoDS_Wire circleWire(double radius,const gp_Pnt&center){return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(center,gp_Dir(0,0,1)),radius)).Edge()).Wire();}
TopoDS_Shape loftCircle(double r0,double r1,double length,gp_Pnt p0=gp_Pnt(0,0,0),gp_Pnt p1=gp_Pnt(0,0,0)){
    p1.SetZ(length);BRepOffsetAPI_ThruSections loft(true,false,1e-6);loft.AddWire(circleWire(r0,p0));loft.AddWire(circleWire(r1,p1));loft.Build();if(!loft.IsDone())throw std::runtime_error("Loft failed in modeling tool.");return loft.Shape();
}
bool integerIn(double v,int lo,int hi){return std::isfinite(v)&&v>=lo&&v<=hi&&std::floor(v)==v;}
} // namespace

EngineeringPartSpec modelingToolSpec(ModelingTool kind){static const QVector<EngineeringPartSpec> specs={
    {"Rectangular tube",{"Outer width (mm)","Outer height (mm)","Wall thickness (mm)","Length (mm)"},{50,30,3,80},1},
    {"Square tube",{"Outer side (mm)","Wall thickness (mm)","Length (mm)"},{40,3,80},2},
    {"Z section beam",{"Flange width (mm)","Overall height (mm)","Web thickness (mm)","Flange thickness (mm)","Length (mm)"},{40,30,3,3,80},3},
    {"Hat channel",{"Width (mm)","Height (mm)","Wall thickness (mm)","Length (mm)"},{50,25,3,80},4},
    {"T-slot rail",{"Width (mm)","Height (mm)","Slot width (mm)","Wall thickness (mm)","Length (mm)"},{40,40,8,4,80},5},
    {"Slotted mounting plate",{"Width (mm)","Height (mm)","Thickness (mm)","Slot length (mm)","Slot width (mm)"},{100,60,6,24,8},6},
    {"Grid-hole plate",{"Width (mm)","Height (mm)","Thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{100,60,6,2,20},7},
    {"Bolt-circle flange",{"Outer radius (mm)","Bore radius (mm)","Thickness (mm)","Bolt-circle radius (mm)","Bolt-hole radius (mm)","Hole count (3-16)"},{30,8,8,22,2,6},8},
    {"Square flange",{"Width (mm)","Bore radius (mm)","Thickness (mm)","Bolt-hole radius (mm)","Hole inset (mm)"},{60,8,8,2,10},9},
    {"Lifting lug",{"Width (mm)","Thickness (mm)","Height (mm)","Pin-hole radius (mm)"},{30,10,50,6},10},
    {"Clevis bracket",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Ear thickness (mm)","Pin-hole radius (mm)"},{40,30,8,6,4},11},
    {"Hinge barrel",{"Barrel radius (mm)","Bore radius (mm)","Length (mm)","Knuckle count (3-9)"},{8,4,48,5},12},
    {"Ribbed plate",{"Width (mm)","Height (mm)","Base thickness (mm)","Rib thickness (mm)","Rib count (2-12)"},{100,60,5,4,4},13},
    {"Triangular gusset",{"Leg length (mm)","Width (mm)","Thickness (mm)"},{40,30,5},14},
    {"Elliptical tube",{"Outer X radius (mm)","Outer Y radius (mm)","Wall thickness (mm)","Length (mm)"},{20,12,3,50},15},
    {"Tapered tube",{"Bottom radius (mm)","Top radius (mm)","Wall thickness (mm)","Length (mm)"},{15,9,2,50},16},
    {"Pipe elbow",{"Centerline radius (mm)","Outer radius (mm)","Wall thickness (mm)","Sweep angle (degrees)"},{25,8,2,90},17},
    {"Pipe tee",{"Outer radius (mm)","Inner radius (mm)","Run length (mm)","Branch length (mm)"},{10,7,60,40},18},
    {"Pipe Y-branch",{"Outer radius (mm)","Inner radius (mm)","Branch length (mm)","Branch angle (degrees)"},{8,5,45,35},19},
    {"Concentric reducer",{"Small-end radius (mm)","Large-end radius (mm)","Wall thickness (mm)","Length (mm)"},{10,16,2,40},20},
    {"Eccentric reducer",{"Small-end radius (mm)","Large-end radius (mm)","Wall thickness (mm)","Length (mm)","Center offset (mm)"},{10,16,2,40,4},21},
    {"Blind flange",{"Outer radius (mm)","Thickness (mm)","Bolt-hole radius (mm)","Bolt-circle radius (mm)","Hole count (3-16)"},{30,8,2,22,6},22},
    {"Flanged pipe",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{12,8,50,22,6},23},
    {"V-groove pulley",{"Outer radius (mm)","Bore radius (mm)","Width (mm)","Groove radius (mm)"},{30,6,12,2},24},
    {"Timing pulley",{"Root radius (mm)","Tip radius (mm)","Width (mm)","Tooth count (12-60)","Bore radius (mm)"},{20,23,12,24,6},25},
    {"Rack gear",{"Length (mm)","Body height (mm)","Tooth height (mm)","Tooth count (4-40)","Face width (mm)"},{100,10,5,12,20},26},
    {"Chain sprocket",{"Root radius (mm)","Tip radius (mm)","Width (mm)","Tooth count (8-60)","Bore radius (mm)"},{20,24,10,20,6},27},
    {"Fan impeller",{"Outer radius (mm)","Hub radius (mm)","Width (mm)","Blade count (3-16)","Blade thickness (mm)"},{35,10,12,6,4},28},
    {"Ball-bearing assembly",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Ball radius (mm)","Ball count (6-16)"},{20,8,10,2,8},29},
    {"Coil spring",{"Mean radius (mm)","Wire radius (mm)","Pitch (mm)","Turns (3-16)"},{15,2,6,8},30}
};const int i=static_cast<int>(kind);return i>=0&&i<specs.size()?specs[i]:EngineeringPartSpec{};}

bool SceneDocument::addModelingTool(ModelingTool kind,const QVector<double>&d,QString*error){
    const auto spec=modelingToolSpec(kind);
    if(spec.name.isEmpty()||d.size()!=spec.defaults.size()){setError(error,"Unknown modeling tool or wrong number of dimensions.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Modeling-tool dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString&m){setError(error,m);return false;};
    if((kind==ModelingTool::RectangularTube&&(2*d[2]>=d[0]||2*d[2]>=d[1]))||
       (kind==ModelingTool::SquareTube&&2*d[1]>=d[0]))return fail("Tube wall thickness does not fit the section.");
    if((kind==ModelingTool::ZSection&&(2*d[3]>=d[1]||d[2]>=d[0]))||
       (kind==ModelingTool::HatChannel&&2*d[2]>=std::min(d[0],d[1]))||
       (kind==ModelingTool::TSlotRail&&(d[2]>=d[0]/2||d[3]>=std::min(d[0],d[1])/2)))return fail("Section dimensions do not fit the profile.");
    if(kind==ModelingTool::SlottedPlate&&(d[4]>=std::min(d[0],d[1])/3||d[3]<=d[4]))return fail("Slot dimensions do not fit the plate.");
    if(kind==ModelingTool::GridHolePlate&&(d[3]*2>=d[4]||d[4]>=std::min(d[0],d[1])/2))return fail("Hole size or spacing does not fit the plate.");
    if((kind==ModelingTool::BoltCircleFlange&&(d[1]>=d[0]||d[3]+d[4]>=d[0]||!integerIn(d[5],3,16)))||
       (kind==ModelingTool::SquareFlange&&(d[1]>=d[0]/2||d[4]<=d[3]||2*d[4]>=d[0]))||
       (kind==ModelingTool::BlindFlange&&(d[2]+d[3]>=d[0]||!integerIn(d[4],3,16))))return fail("Flange bore, bolt circle, or hole count is invalid.");
    if(kind==ModelingTool::LiftingLug&&(d[3]>=std::min(d[0],d[2])/3))return fail("Lifting-lug pin hole does not fit the plate.");
    if(kind==ModelingTool::ClevisBracket&&(d[3]>=d[1]/3||d[4]>=d[2]))return fail("Clevis ears or pin hole do not fit the bracket.");
    if(kind==ModelingTool::HingeBarrel&&(d[1]>=d[0]||!integerIn(d[3],3,9)))return fail("Hinge bore or knuckle count is invalid.");
    if(kind==ModelingTool::RibbedPlate&&(d[3]>=d[0]/4||!integerIn(d[4],2,12)))return fail("Rib thickness or count is invalid.");
    if(kind==ModelingTool::EllipticalTube&&(d[2]>=std::min(d[0],d[1])))return fail("Elliptical-tube wall is too thick.");
    if(kind==ModelingTool::TaperedTube&&(d[2]>=std::min(d[0],d[1])))return fail("Tapered-tube wall must leave a bore at both ends.");
    if(kind==ModelingTool::PipeElbow&&(d[0]<=d[1]||d[2]>=d[1]||d[3]>180))return fail("Pipe-elbow radii or sweep angle are invalid.");
    if((kind==ModelingTool::PipeTee||kind==ModelingTool::PipeYBranch)&&(d[1]>=d[0]))return fail("Pipe bore must be smaller than the outside radius.");
    if(kind==ModelingTool::PipeYBranch&&(d[3]>=80))return fail("Y-branch angle must be less than 80 degrees.");
    if((kind==ModelingTool::ConcentricReducer||kind==ModelingTool::EccentricReducer)&&(d[0]<=d[2]||d[1]<=d[2]||d[0]>=d[1]))return fail("Reducer dimensions must leave valid bores and increasing end sizes.");
    if(kind==ModelingTool::EccentricReducer&&d[4]>=d[0]-d[2])return fail("Eccentric offset is too large for the small end.");
    if(kind==ModelingTool::FlangedPipe&&(d[1]>=d[0]||d[3]<=d[0]||d[4]>=d[2]))return fail("Flanged-pipe dimensions do not fit.");
    if(kind==ModelingTool::VGroovePulley&&(d[1]>=d[0]||d[3]>=d[0]/2))return fail("Pulley bore or groove does not fit.");
    if((kind==ModelingTool::TimingPulley||kind==ModelingTool::ChainSprocket)&&(!integerIn(d[3],kind==ModelingTool::TimingPulley?12:8,60)||d[0]>=d[1]||d[4]>=d[0]))return fail("Tooth count, radii, or bore are invalid.");
    if(kind==ModelingTool::RackGear&&(!integerIn(d[3],4,40)||d[2]>=d[1]))return fail("Rack tooth dimensions or count are invalid.");
    if(kind==ModelingTool::FanImpeller&&(d[1]>=d[0]*0.6||!integerIn(d[3],3,16)||d[4]>=d[0]-d[1]))return fail("Impeller hub or blades do not fit.");
    if(kind==ModelingTool::BallBearingAssembly&&(d[1]>=d[0]*0.7||d[3]>=((d[0]-d[1])/4)||!integerIn(d[4],6,16)))return fail("Bearing races, balls, or count are invalid.");
    if(kind==ModelingTool::CoilSpring&&(d[1]>=d[0]/3||d[2]<=2*d[1]||!integerIn(d[3],3,16)))return fail("Spring wire, pitch, or turn count is invalid.");
    try{
        TopoDS_Shape shape;
        switch(kind){
        case ModelingTool::RectangularTube:{auto outer=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[3]);shape=cut(outer,box(-d[0]/2+d[2],-d[1]/2+d[2],-1,d[0]-2*d[2],d[1]-2*d[2],d[3]+2));break;}
        case ModelingTool::SquareTube:{auto outer=box(0,0,0,d[0],d[0],d[2]);shape=cut(outer,box(d[1],d[1],-1,d[0]-2*d[1],d[0]-2*d[1],d[2]+2));break;}
        case ModelingTool::ZSection:{const double w=d[0],h=d[1],t=d[2],f=d[3];shape=prism({gp_Pnt(0,0,0),gp_Pnt(w,0,0),gp_Pnt(w,h-f,0),gp_Pnt(w-t,h-f,0),gp_Pnt(w-t,t,0),gp_Pnt(t,t,0),gp_Pnt(t,h,0),gp_Pnt(0,h,0)},d[4]);break;}
        case ModelingTool::HatChannel:{const double w=d[0],h=d[1],t=d[2];auto section=box(0,0,0,w,t,d[3]);section=fuse(section,box(0,t,0,t,h-t,d[3]));section=fuse(section,box(w-t,t,0,t,h-t,d[3]));section=fuse(section,box(t,h-t,0,2*t,t,d[3]));section=fuse(section,box(w-3*t,h-t,0,2*t,t,d[3]));shape=section;break;}
        case ModelingTool::TSlotRail:{const double w=d[0],h=d[1],s=d[2],t=d[3];shape=prism({gp_Pnt(0,0,0),gp_Pnt(w,0,0),gp_Pnt(w,h,0),gp_Pnt((w+s)/2+t,h,0),gp_Pnt((w+s)/2+t,h-t,0),gp_Pnt((w-s)/2,h-t,0),gp_Pnt((w-s)/2,h,0),gp_Pnt(0,h,0)},d[4]);break;}
        case ModelingTool::SlottedPlate:{auto plate=box(0,0,0,d[0],d[1],d[2]);const double r=d[4]/2,straight=d[3]-d[4];for(double x:{d[0]*0.25,d[0]*0.75}){auto slot=cylinder(r,d[2]+2,gp_Pnt(x-straight/2,d[1]/2,-1));slot=fuse(slot,cylinder(r,d[2]+2,gp_Pnt(x+straight/2,d[1]/2,-1)));slot=fuse(slot,box(x-straight/2,d[1]/2-r,-1,straight,d[4],d[2]+2));plate=cut(plate,slot);}shape=plate;break;}
        case ModelingTool::GridHolePlate:{auto plate=box(0,0,0,d[0],d[1],d[2]);const int nx=std::max(1,static_cast<int>(std::floor(d[0]/d[4]))-1),ny=std::max(1,static_cast<int>(std::floor(d[1]/d[4]))-1);for(int ix=0;ix<nx;++ix)for(int iy=0;iy<ny;++iy){double x=(ix+1)*d[4],y=(iy+1)*d[4];plate=cut(plate,cylinder(d[3],d[2]+2,gp_Pnt(x,y,-1)));}shape=plate;break;}
        case ModelingTool::BoltCircleFlange:{auto plate=cylinder(d[0],d[2]);plate=cut(plate,cylinder(d[1],d[2]+2,gp_Pnt(0,0,-1)));for(int i=0;i<static_cast<int>(d[5]);++i){double a=2*pi*i/d[5];plate=cut(plate,cylinder(d[4],d[2]+2,gp_Pnt(d[3]*std::cos(a),d[3]*std::sin(a),-1)));}shape=plate;break;}
        case ModelingTool::SquareFlange:{auto plate=box(0,0,0,d[0],d[0],d[2]);plate=cut(plate,cylinder(d[1],d[2]+2,gp_Pnt(d[0]/2,d[0]/2,-1)));for(double x:{d[4],d[0]-d[4]})for(double y:{d[4],d[0]-d[4]})plate=cut(plate,cylinder(d[3],d[2]+2,gp_Pnt(x,y,-1)));shape=plate;break;}
        case ModelingTool::LiftingLug:{auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]-d[0]/2);body=fuse(body,BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0,0,d[2]-d[0]/2),gp_Dir(0,1,0)),d[0]/2,d[1]).Shape());shape=cut(body,cylinder(d[3],d[1]+2,gp_Pnt(0,-d[1]/2-1,d[2]-d[0]/2),gp_Dir(0,1,0)));break;}
        case ModelingTool::ClevisBracket:{auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);double earHeight=d[1]*0.65;for(double y:{-d[1]/2+d[3]/2,d[1]/2-d[3]/2}){auto ear=box(-d[0]/2,y-d[3]/2,d[2],d[0],d[3],earHeight);ear=fuse(ear,cylinder(d[0]/2,d[3],gp_Pnt(0,y-d[3]/2,d[2]+earHeight),gp_Dir(0,1,0)));body=fuse(body,ear);body=cut(body,cylinder(d[4],d[3]+2,gp_Pnt(0,y-d[3]/2-1,d[2]+earHeight),gp_Dir(0,1,0)));}shape=body;break;}
        case ModelingTool::HingeBarrel:{BRep_Builder builder;TopoDS_Compound hinge;builder.MakeCompound(hinge);const int count=static_cast<int>(d[3]);const double segment=d[2]/count,knuckleLength=segment*0.82;for(int i=0;i<count;++i){const double z=i*segment;auto knuckle=cylinder(d[0],knuckleLength,gp_Pnt(0,0,z));knuckle=cut(knuckle,cylinder(d[1],knuckleLength+2,gp_Pnt(0,0,z-1)));builder.Add(hinge,knuckle);if(i%2==0)builder.Add(hinge,box(d[0]-1,-d[2]/2,z,d[0]*1.8,d[2],knuckleLength));}shape=hinge;break;}
        case ModelingTool::RibbedPlate:{auto plate=box(0,0,0,d[0],d[1],d[2]);for(int i=0;i<static_cast<int>(d[4]);++i){double x=(i+1)*d[0]/(d[4]+1);plate=fuse(plate,box(x-d[3]/2,0,d[2],d[3],d[1],d[2]));}shape=plate;break;}
        case ModelingTool::TriangularGusset:shape=prism({gp_Pnt(0,0,0),gp_Pnt(d[0],0,0),gp_Pnt(0,d[0],0)},d[1]);shape=fuse(shape,box(0,0,0,d[0],d[2],d[1]));break;
        case ModelingTool::EllipticalTube:{QVector<gp_Pnt> outer,inner;constexpr int n=64;for(int i=0;i<n;++i){double a=2*pi*i/n;outer.append(gp_Pnt(d[0]*std::cos(a),d[1]*std::sin(a),0));inner.append(gp_Pnt((d[0]-d[2])*std::cos(a),(d[1]-d[2])*std::sin(a),0));}auto shell=prism(outer,d[3]);shape=cut(shell,prism(inner,d[3]+2));break;}
        case ModelingTool::TaperedTube:{auto outer=BRepPrimAPI_MakeCone(d[0],d[1],d[3]).Shape();auto inner=BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0,0,-1),gp_Dir(0,0,1)),d[0]-d[2],d[1]-d[2],d[3]+2).Shape();shape=cut(outer,inner);break;}
        case ModelingTool::PipeElbow:{const double angle=d[3]*pi/180.0;auto outer=BRepPrimAPI_MakeTorus(d[0],d[1],angle).Shape();auto inner=BRepPrimAPI_MakeTorus(d[0],d[1]-d[2],angle).Shape();shape=cut(outer,inner);break;}
        case ModelingTool::PipeTee:{auto run=cylinder(d[0],d[2],gp_Pnt(0,0,-d[2]/2));auto branch=cylinder(d[0],d[3],gp_Pnt(0,0,0),gp_Dir(1,0,0));auto shell=fuse(run,branch);auto runBore=cylinder(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1));auto branchBore=cylinder(d[1],d[3]+2,gp_Pnt(-1,0,0),gp_Dir(1,0,0));shape=cut(shell,fuse(runBore,branchBore));break;}
        case ModelingTool::PipeYBranch:{const double a=d[3]*pi/180.0;auto outer=cylinder(d[0],d[2],gp_Pnt(0,0,-d[2]/2));auto left=cylinder(d[0],d[2]*0.7,gp_Pnt(0,0,0),gp_Dir(-std::sin(a),0,std::cos(a)));auto right=cylinder(d[0],d[2]*0.7,gp_Pnt(0,0,0),gp_Dir(std::sin(a),0,std::cos(a)));auto shell=fuse(fuse(outer,left),right);auto inner=cylinder(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1));auto leftBore=cylinder(d[1],d[2]*0.7+2,gp_Pnt(0,0,0),gp_Dir(-std::sin(a),0,std::cos(a)));auto rightBore=cylinder(d[1],d[2]*0.7+2,gp_Pnt(0,0,0),gp_Dir(std::sin(a),0,std::cos(a)));shape=cut(shell,fuse(fuse(inner,leftBore),rightBore));break;}
        case ModelingTool::ConcentricReducer:shape=cut(loftCircle(d[1],d[0],d[3]),loftCircle(d[1]-d[2],d[0]-d[2],d[3]+2,gp_Pnt(0,0,-1)));break;
        case ModelingTool::EccentricReducer:{auto outer=loftCircle(d[1],d[0],d[3],gp_Pnt(0,0,0),gp_Pnt(d[4],0,0));auto inner=loftCircle(d[1]-d[2],d[0]-d[2],d[3]+2,gp_Pnt(0,0,-1),gp_Pnt(d[4],0,-1));shape=cut(outer,inner);break;}
        case ModelingTool::BlindFlange:{auto plate=cylinder(d[0],d[1]);for(int i=0;i<static_cast<int>(d[4]);++i){double a=2*pi*i/d[4];plate=cut(plate,cylinder(d[2],d[1]+2,gp_Pnt(d[3]*std::cos(a),d[3]*std::sin(a),-1)));}shape=plate;break;}
        case ModelingTool::FlangedPipe:{auto body=cylinder(d[0],d[2]);body=cut(body,cylinder(d[1],d[2]+2,gp_Pnt(0,0,-1)));auto flange=cylinder(d[3],d[4],gp_Pnt(0,0,d[2]-d[4]));flange=cut(flange,cylinder(d[1],d[4]+2,gp_Pnt(0,0,d[2]-d[4]-1)));shape=fuse(body,flange);break;}
        case ModelingTool::VGroovePulley:{auto blank=cylinder(d[0],d[2]);auto groove=BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0,0,d[2]/2),gp_Dir(0,0,1)),d[0]-d[3],d[3]).Shape();blank=cut(blank,groove);shape=cut(blank,cylinder(d[1],d[2]+2,gp_Pnt(0,0,-1)));break;}
        case ModelingTool::TimingPulley:shape=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case ModelingTool::RackGear:{auto rack=box(0,0,0,d[0],d[4],d[1]);const double toothWidth=d[0]/d[3];for(int i=0;i<static_cast<int>(d[3]);++i)rack=fuse(rack,box(i*toothWidth,0,d[1],toothWidth*0.65,d[4],d[2]));shape=rack;break;}
        case ModelingTool::ChainSprocket:shape=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case ModelingTool::FanImpeller:{auto hub=cylinder(d[1],d[2]);const int count=static_cast<int>(d[3]);for(int i=0;i<count;++i){const double a=2*pi*i/count;auto blade=box(d[1]*0.55,-d[4]/2,d[2]*0.15,d[0]-d[1]*0.55,d[4],d[2]*0.7);gp_Trsf tr;tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),a);hub=fuse(hub,BRepBuilderAPI_Transform(blade,tr,true).Shape());}shape=hub;break;}
        case ModelingTool::BallBearingAssembly:{BRep_Builder builder;TopoDS_Compound assembly;builder.MakeCompound(assembly);auto outer=cylinder(d[0],d[2]);auto outerBore=cylinder(d[0]-d[3]*1.5,d[2]+2,gp_Pnt(0,0,-1));builder.Add(assembly,cut(outer,outerBore));auto inner=cylinder(d[1]+d[3]*1.5,d[2]);builder.Add(assembly,cut(inner,cylinder(d[1],d[2]+2,gp_Pnt(0,0,-1))));double pitch=(d[0]+d[1])/2;for(int i=0;i<static_cast<int>(d[4]);++i){double a=2*pi*i/d[4];builder.Add(assembly,BRepPrimAPI_MakeSphere(gp_Pnt(pitch*std::cos(a),pitch*std::sin(a),d[2]/2),d[3]).Shape());}shape=assembly;break;}
        case ModelingTool::CoilSpring:{const int turns=static_cast<int>(d[3]),samples=turns*16;Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,samples+1);for(int i=0;i<=samples;++i){double u=static_cast<double>(i)/samples,a=2*pi*turns*u;points->SetValue(i+1,gp_Pnt(d[0]*std::cos(a),d[0]*std::sin(a),d[2]*turns*u));}GeomAPI_Interpolate interpolation(points,false,1e-5);interpolation.Perform();if(!interpolation.IsDone())throw std::runtime_error("Spring centerline interpolation failed.");BRepBuilderAPI_MakeEdge edge(interpolation.Curve());TopoDS_Wire path=BRepBuilderAPI_MakeWire(edge.Edge()).Wire();gp_Pnt start(d[0],0,0);TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(start,gp_Dir(0,1,0)),d[1])).Edge()).Wire();BRepOffsetAPI_MakePipe pipe(path,profile);pipe.Build();if(!pipe.IsDone())throw std::runtime_error("Spring sweep failed.");shape=pipe.Shape();break;}
        default:return fail("Unsupported modeling tool.");
        }
        if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid())return fail("Generated modeling-tool shape failed topology validation.");
        GProp_GProps props;BRepGProp::VolumeProperties(shape,props);if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated modeling tool has no positive volume.");
        checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),shape,true,"#6f83b5"});return true;
    }catch(const Standard_Failure&e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}catch(const std::exception&e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
