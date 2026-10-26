#include "SceneDocument.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cad {
namespace {
using S = SpecializedPart;
constexpr double pi = std::numbers::pi;

TopoDS_Shape cylinder(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir a=gp_Dir(0,0,1)) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(p,a),r,h).Shape();
}
TopoDS_Shape fuse(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not join specialized component features.");
    return op.Shape();
}
TopoDS_Shape cut(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not cut specialized component feature.");
    return op.Shape();
}
TopoDS_Shape ring(double ro,double ri,double h,double z=0) {
    auto s=cylinder(ro,h,gp_Pnt(0,0,z));
    return ri>0?cut(s,cylinder(ri,h+0.02,gp_Pnt(0,0,z-0.01))):s;
}
TopoDS_Shape box(double x,double y,double z,double dx,double dy,double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),dx,dy,dz).Shape();
}
TopoDS_Shape radialGear(double root,double tip,int teeth,double width,double bore) {
    BRepBuilderAPI_MakePolygon p;
    for(int i=0;i<teeth;++i){
        const double a=2*pi*i/teeth,half=pi/(2*teeth);
        p.Add(gp_Pnt(root*cos(a-2*half),root*sin(a-2*half),0));
        p.Add(gp_Pnt(tip*cos(a-half),tip*sin(a-half),0));
        p.Add(gp_Pnt(tip*cos(a+half),tip*sin(a+half),0));
        p.Add(gp_Pnt(root*cos(a+2*half),root*sin(a+2*half),0));
    }
    p.Close();
    if(!p.IsDone())throw std::runtime_error("Gear profile is invalid.");
    auto body=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(p.Wire()).Face(),gp_Vec(0,0,width)).Shape();
    return bore>0?cut(body,cylinder(bore,width+0.02,gp_Pnt(0,0,-0.01))):body;
}
void addCompound(TopoDS_Compound& c,const TopoDS_Shape& s) { BRep_Builder b;b.Add(c,s); }
TopoDS_Compound compound() { TopoDS_Compound c;BRep_Builder b;b.MakeCompound(c);return c; }
TopoDS_Shape annularAlongX(double ro,double ri,double len,double x=0) {
    auto s=cylinder(ro,len,gp_Pnt(x-len/2,0,0),gp_Dir(1,0,0));
    return ri>0?cut(s,cylinder(ri,len+0.02,gp_Pnt(x-len/2-0.01,0,0),gp_Dir(1,0,0))):s;
}
TopoDS_Shape rotatedBox(double x,double y,double z,double dx,double dy,double dz,double angle) {
    auto s=box(x,y,z,dx,dy,dz);gp_Trsf t;t.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),angle);
    return BRepBuilderAPI_Transform(s,t,true).Shape();
}
bool isCount(double x,int lo,int hi){return std::isfinite(x)&&x>=lo&&x<=hi&&std::floor(x)==x;}
}

EngineeringPartSpec specializedPartSpec(SpecializedPart k) {
    switch(k){
    case S::HelicalGear:return {"Helical gear",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Teeth (12-60)","Bore radius (mm)"},{18,21,14,24,5}};
    case S::BevelGear:return {"Bevel gear",{"Base radius (mm)","Tip radius (mm)","Face width (mm)","Teeth (12-48)","Bore radius (mm)"},{18,21,14,24,5}};
    case S::WormWheel:return {"Worm wheel",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Teeth (12-60)","Bore radius (mm)"},{20,23,12,30,6}};
    case S::WormShaft:return {"Worm shaft",{"Shaft radius (mm)","Thread crest radius (mm)","Length (mm)","Thread pitch (mm)","Thread ridge count (4-30)"},{6,9,50,7,6}};
    case S::PlanetCarrier:return {"Planet carrier",{"Carrier radius (mm)","Plate thickness (mm)","Planet pin radius (mm)","Pin circle radius (mm)","Planet count (3-6)"},{28,6,3,18,3}};
    case S::PlanetaryGearSet:return {"Planetary gear set",{"Sun radius (mm)","Planet radius (mm)","Ring inner radius (mm)","Face width (mm)","Planet count (3-6)"},{8,6,22,10,4}};
    case S::IdlerPulley:return {"Idler pulley",{"Outer radius (mm)","Bore radius (mm)","Width (mm)","Flange radius (mm)"},{18,5,12,21}};
    case S::TimingBeltTensioner:return {"Timing belt tensioner",{"Base width (mm)","Base height (mm)","Wheel radius (mm)","Wheel width (mm)","Axle radius (mm)"},{42,8,14,10,4}};
    case S::NeedleBearing:return {"Needle roller bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Needle radius (mm)","Needle count (8-24)"},{18,12,12,1.2,14}};
    case S::TaperedRollerBearing:return {"Tapered roller bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Roller radius (mm)","Roller count (8-20)"},{22,12,14,2.4,12}};
    case S::ThrustBearing:return {"Thrust bearing",{"Outer radius (mm)","Inner radius (mm)","Race thickness (mm)","Ball radius (mm)","Ball count (8-24)"},{20,8,3,2,12}};
    case S::SphericalRollerBearing:return {"Spherical roller bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Roller radius (mm)","Roller count (8-20)"},{24,12,16,2.5,12}};
    case S::HydraulicCylinder:return {"Hydraulic cylinder",{"Barrel outer radius (mm)","Bore radius (mm)","Stroke (mm)","Rod radius (mm)","Cap thickness (mm)"},{18,14,60,5,8}};
    case S::PneumaticCylinder:return {"Pneumatic cylinder",{"Barrel outer radius (mm)","Bore radius (mm)","Stroke (mm)","Rod radius (mm)","Cap thickness (mm)"},{16,12,50,4,7}};
    case S::LinearActuator:return {"Linear actuator",{"Housing radius (mm)","Screw radius (mm)","Body length (mm)","Rod extension (mm)","Mount radius (mm)"},{20,5,70,45,8}};
    case S::TieRodCylinder:return {"Tie-rod cylinder",{"Barrel radius (mm)","Bore radius (mm)","Body length (mm)","Rod radius (mm)","Tie-rod radius (mm)"},{18,13,65,5,2}};
    case S::Crankshaft:return {"Crankshaft",{"Main journal radius (mm)","Crank throw (mm)","Journal length (mm)","Web radius (mm)","Overall length (mm)"},{5,12,12,11,70}};
    case S::ConnectingRod:return {"Connecting rod",{"Small-end radius (mm)","Big-end radius (mm)","Center distance (mm)","Thickness (mm)","Bore radius (mm)"},{8,14,60,8,4}};
    case S::Piston:return {"Piston",{"Outer radius (mm)","Pin-bore radius (mm)","Height (mm)","Crown thickness (mm)","Ring groove width (mm)"},{16,4,32,5,2}};
    case S::PistonRing:return {"Piston ring",{"Outer radius (mm)","Inner radius (mm)","Axial width (mm)","Gap angle (degrees)"},{16,14.5,2,18}};
    case S::CylinderLiner:return {"Cylinder liner",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange thickness (mm)"},{18,14,60,22,5}};
    case S::CamFollower:return {"Cam follower",{"Roller radius (mm)","Roller width (mm)","Stud radius (mm)","Stud length (mm)","Head radius (mm)"},{10,12,4,24,7}};
    case S::EccentricCam:return {"Eccentric cam",{"Cam radius (mm)","Bore radius (mm)","Eccentricity (mm)","Thickness (mm)"},{18,5,4,10}};
    case S::ChainLink:return {"Chain link",{"Pitch (mm)","Plate width (mm)","Plate thickness (mm)","Pin radius (mm)","Roller radius (mm)"},{20,10,3,2,4}};
    case S::RollerChainSegment:return {"Roller chain segment",{"Pitch (mm)","Plate width (mm)","Plate thickness (mm)","Pin radius (mm)","Link count (3-12)"},{20,10,3,2,5}};
    case S::DoubleRowSprocket:return {"Double-row sprocket",{"Root radius (mm)","Tip radius (mm)","Row width (mm)","Teeth (12-48)","Bore radius (mm)"},{20,23,8,24,6}};
    case S::FlexibleBellowsCoupling:return {"Flexible bellows coupling",{"Bore radius (mm)","Outer radius (mm)","Length (mm)","Rib depth (mm)","Rib count (4-16)"},{5,14,32,2,8}};
    case S::DiscCoupling:return {"Disc coupling",{"Bore radius (mm)","Hub radius (mm)","Hub length (mm)","Disc radius (mm)","Disc thickness (mm)"},{5,12,14,22,2}};
    case S::OldhamCoupling:return {"Oldham coupling",{"Bore radius (mm)","Hub radius (mm)","Hub length (mm)","Center disc radius (mm)","Disc thickness (mm)"},{5,12,12,18,6}};
    case S::YStrainer:return {"Y strainer",{"Run outer radius (mm)","Bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{16,10,48,12,34}};
    case S::HoseBarb:return {"Hose barb fitting",{"Tube outer radius (mm)","Bore radius (mm)","Length (mm)","Barb radius (mm)","Barb count (2-8)"},{8,5,34,10,4}};
    case S::PipeElbow:return {"Pipe elbow",{"Outer radius (mm)","Bore radius (mm)","Leg length (mm)","Bend radius (mm)"},{12,8,30,22}};
    case S::PipeCross:return {"Pipe cross fitting",{"Outer radius (mm)","Bore radius (mm)","Run length (mm)","Branch length (mm)"},{12,8,50,24}};
    case S::ExpansionJoint:return {"Pipe expansion joint",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Corrugation count (3-12)"},{14,9,45,20,6}};
    case S::CoolingFan:return {"Axial cooling fan",{"Frame radius (mm)","Hub radius (mm)","Thickness (mm)","Blade count (4-12)","Blade width (mm)"},{35,10,12,7,8}};
    case S::Count:break;
    }
    return {};
}

bool SceneDocument::addSpecializedPart(SpecializedPart kind,const QVector<double>& d,QString* error) {
    const auto spec=specializedPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=spec.defaults.size()){setError(error,"Unknown specialized component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Specialized component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString& m){setError(error,m);return false;};
    if(((kind==S::HelicalGear||kind==S::BevelGear||kind==S::WormWheel||kind==S::DoubleRowSprocket)&&!isCount(d[3],12,60))||
       (kind==S::WormShaft&&!isCount(d[4],4,30))||(kind==S::PlanetCarrier&&!isCount(d[4],3,6))||
       (kind==S::PlanetaryGearSet&&!isCount(d[4],3,6))||(kind==S::NeedleBearing&&!isCount(d[4],8,24))||
       ((kind==S::TaperedRollerBearing||kind==S::SphericalRollerBearing)&&!isCount(d[4],8,20))||
       (kind==S::ThrustBearing&&!isCount(d[4],8,24))||(kind==S::RollerChainSegment&&!isCount(d[4],3,12))||
       (kind==S::FlexibleBellowsCoupling&&!isCount(d[4],4,16))||(kind==S::HoseBarb&&!isCount(d[4],2,8))||
       (kind==S::ExpansionJoint&&!isCount(d[4],3,12))||(kind==S::CoolingFan&&!isCount(d[3],4,12)))
        return fail("Count values must be whole numbers within the dialog range.");
    if((kind==S::HelicalGear||kind==S::BevelGear||kind==S::WormWheel||kind==S::DoubleRowSprocket)&&
       (d[0]>=d[1]||d[4]>=d[0]))return fail("Gear root, tip and bore radii do not fit.");
    if(kind==S::WormShaft&&(d[0]>=d[1]||d[1]*2>=d[2]||d[3]<=d[0]))return fail("Worm crest and pitch do not fit the shaft.");
    if(kind==S::PlanetCarrier&&(d[2]>=d[0]/3||d[3]+d[2]>=d[0]))return fail("Planet pins must fit within the carrier plate.");
    if(kind==S::PlanetaryGearSet&&(d[0]+2*d[1]>=d[2]||d[2]<=d[0]))return fail("Planetary gear radii do not fit the ring.");
    if((kind==S::IdlerPulley)&&(d[1]>=d[0]||d[3]<=d[0]))return fail("Idler bore and flange must fit the wheel.");
    if((kind==S::NeedleBearing||kind==S::TaperedRollerBearing||kind==S::ThrustBearing||kind==S::SphericalRollerBearing)&&
       (d[1]>=d[0]||d[3]>=(d[0]-d[1])/2))return fail("Bearing races and rolling elements do not fit.");
    if((kind==S::HydraulicCylinder||kind==S::PneumaticCylinder||kind==S::TieRodCylinder)&&
       (d[1]>=d[0]||d[3]>=d[1]))return fail("Cylinder bore and rod must fit inside the barrel.");
    if(kind==S::LinearActuator&&(d[1]>=d[0]||d[4]>=d[0]))return fail("Actuator screw and mount must fit the housing.");
    if(kind==S::Crankshaft&&(d[1]+d[0]>=d[3]*2||d[4]<d[2]*3))return fail("Crank throw and journals do not fit the shaft.");
    if(kind==S::ConnectingRod&&(d[0]>=d[1]||d[4]>=d[0]||d[2]<=d[0]+d[1]))return fail("Connecting rod end sizes do not fit the center distance.");
    if(kind==S::Piston&&(d[1]>=d[0]||d[3]>=d[2]||d[4]>=d[2]/4))return fail("Piston dimensions do not fit.");
    if(kind==S::PistonRing&&(d[1]>=d[0]||d[3]>=90))return fail("Ring gap and bore dimensions do not fit.");
    if(kind==S::CylinderLiner&&(d[1]>=d[0]||d[3]<=d[0]))return fail("Cylinder liner bore and flange do not fit.");
    if(kind==S::EccentricCam&&(d[1]+d[2]>=d[0]))return fail("Cam bore and eccentric offset exceed the cam radius.");
    if((kind==S::ChainLink||kind==S::RollerChainSegment)&&(d[3]>=d[0]/3||d[4]>=d[0]/2))return fail("Chain pins or rollers do not fit the pitch.");
    if(kind==S::FlexibleBellowsCoupling&&(d[0]>=d[1]-d[3]))return fail("Bellows bore and rib depth do not fit.");
    if((kind==S::DiscCoupling||kind==S::OldhamCoupling)&&(d[0]>=d[1]||d[1]>=d[3]))return fail("Coupling bore, hub and disc dimensions do not fit.");
    if((kind==S::YStrainer||kind==S::HoseBarb||kind==S::PipeElbow||kind==S::PipeCross||kind==S::ExpansionJoint)&&d[1]>=d[0])return fail("Pipe bore must be smaller than its outside radius.");
    if(kind==S::HoseBarb&&d[3]<=d[0])return fail("Barb radius must exceed the tube radius.");
    if(kind==S::ExpansionJoint&&d[3]<=d[0])return fail("Expansion-joint flange must exceed the bellows radius.");
    if(kind==S::CoolingFan&&(d[1]>=d[0]*0.6||d[4]>=d[0]-d[1]))return fail("Fan hub or blades do not fit the frame.");

    try {
        TopoDS_Shape s;
        switch(kind){
        case S::HelicalGear:s=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case S::BevelGear:{auto c=BRepPrimAPI_MakeCone(d[1],d[0],d[2]).Shape();s=d[4]>0?cut(c,cylinder(d[4],d[2]+0.02,gp_Pnt(0,0,-0.01))):c;break;}
        case S::WormWheel:s=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case S::WormShaft:{auto c=cylinder(d[0],d[2]);TopoDS_Compound assembly=compound();addCompound(assembly,c);for(int i=0;i<static_cast<int>(d[4]);++i)addCompound(assembly,ring(d[1],d[0]*0.9,d[3]*0.3,d[2]*0.12+i*d[3]));s=assembly;break;}
        case S::PlanetCarrier:{auto plate=ring(d[0],d[2]*1.3,d[1]);for(int i=0;i<static_cast<int>(d[4]);++i){double a=2*pi*i/d[4];plate=fuse(plate,cylinder(d[2],d[1],gp_Pnt(d[3]*cos(a),d[3]*sin(a),0)));}s=plate;break;}
        case S::PlanetaryGearSet:{TopoDS_Compound a=compound();addCompound(a,ring(d[2],d[2]-2*d[1],d[3]));addCompound(a,radialGear(d[0]*0.8,d[0],20,d[3],d[0]*0.35));for(int i=0;i<static_cast<int>(d[4]);++i){double t=2*pi*i/d[4];gp_Trsf tr;tr.SetTranslation(gp_Vec((d[0]+d[1])*cos(t),(d[0]+d[1])*sin(t),0));addCompound(a,BRepBuilderAPI_Transform(radialGear(d[1]*0.8,d[1],16,d[3],d[1]*0.25),tr,true).Shape());}s=a;break;}
        case S::IdlerPulley:{auto wheel=ring(d[0],d[1],d[2]);wheel=fuse(wheel,cylinder(d[3],d[1],gp_Pnt(0,0,0)));wheel=fuse(wheel,cylinder(d[3],d[1],gp_Pnt(0,0,d[2]-d[1])));s=wheel;break;}
        case S::TimingBeltTensioner:{auto base=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[1]);auto wheel=ring(d[2],d[4],d[3],d[1]*0.5);s=fuse(base,wheel);break;}
        case S::NeedleBearing:case S::TaperedRollerBearing:case S::ThrustBearing:case S::SphericalRollerBearing:{
            const bool thrust=kind==S::ThrustBearing;auto a=ring(d[0],d[1],thrust?d[2]:d[2]);TopoDS_Compound ass=compound();addCompound(ass,a);
            if(thrust)addCompound(ass,ring(d[0]*0.88,d[1]*1.12,d[2],d[2]));
            for(int i=0;i<static_cast<int>(d[4]);++i){double t=2*pi*i/d[4],rr=(d[0]+d[1])/2;addCompound(ass,cylinder(d[3],d[2]*0.65,gp_Pnt(rr*cos(t),rr*sin(t),d[2]*0.18)));}
            s=ass;break;}
        case S::HydraulicCylinder:case S::PneumaticCylinder:{const double h=d[2]+2*d[4];TopoDS_Compound c=compound();addCompound(c,ring(d[0],d[1],h));addCompound(c,cylinder(d[1]*0.95,d[4],gp_Pnt(0,0,-d[4])));addCompound(c,cylinder(d[1]*0.95,d[4],gp_Pnt(0,0,h)));addCompound(c,cylinder(d[3],d[2]+d[4],gp_Pnt(0,0,h)));s=c;break;}
        case S::LinearActuator:{TopoDS_Compound c=compound();addCompound(c,ring(d[0],d[1],d[2]));addCompound(c,cylinder(d[1]*0.9,d[3],gp_Pnt(0,0,d[2])));addCompound(c,cylinder(d[4],d[2]*0.25,gp_Pnt(0,0,0)));s=c;break;}
        case S::TieRodCylinder:{TopoDS_Compound c=compound();addCompound(c,ring(d[0],d[1],d[2]));addCompound(c,cylinder(d[1]*0.9,d[2]*0.15,gp_Pnt(0,0,-d[2]*0.15)));addCompound(c,cylinder(d[1]*0.9,d[2]*0.15,gp_Pnt(0,0,d[2])));addCompound(c,cylinder(d[3],d[2]*0.7,gp_Pnt(0,0,d[2]*0.15)));for(int i=0;i<4;++i){double a=pi/4+i*pi/2;addCompound(c,cylinder(d[4],d[2]+d[2]*0.3,gp_Pnt((d[0]+d[4])*cos(a),(d[0]+d[4])*sin(a),-d[2]*0.15)));}s=c;break;}
        case S::Crankshaft:{auto shaft=cylinder(d[0],d[4],gp_Pnt(0,0,-d[4]/2));TopoDS_Compound c=compound();addCompound(c,shaft);for(int i=0;i<2;++i){double z=(i?1:-1)*d[4]*0.2;addCompound(c,cylinder(d[0]*1.4,d[2],gp_Pnt(0,0,z-d[2]/2)));addCompound(c,cylinder(d[0]*1.3,d[2],gp_Pnt(d[1],0,z+d[2]/2)));addCompound(c,box(0,-d[3]/3,z-d[2]/2,d[1],2*d[3]/3,d[2]));}s=c;break;}
        case S::ConnectingRod:{TopoDS_Compound c=compound();addCompound(c,cylinder(d[1],d[3],gp_Pnt(d[2]/2,0,0)));addCompound(c,cylinder(d[0],d[3],gp_Pnt(-d[2]/2,0,0)));addCompound(c,box(-d[2]/2,-d[0]*0.55,0,d[2],d[0]*1.1,d[3]));s=cut(c,cylinder(d[4],d[3]+0.02,gp_Pnt(-d[2]/2,0,-0.01)));s=cut(s,cylinder(d[4],d[3]+0.02,gp_Pnt(d[2]/2,0,-0.01)));break;}
        case S::Piston:{auto body=cylinder(d[0],d[2]);body=cut(body,cylinder(d[1],2*d[0],gp_Pnt(0,-d[0],d[2]*0.4),gp_Dir(0,1,0)));for(double z:{d[2]*0.68,d[2]*0.78,d[2]*0.88})body=cut(body,ring(d[0]+0.02,d[0]-d[4],d[4],z));s=body;break;}
        case S::PistonRing:{auto r=ring(d[0],d[1],d[2]);auto gap=BRepPrimAPI_MakeBox(gp_Pnt(-d[0],-d[0]*0.05,0),d[0]*2,d[0]*0.1,d[2]+0.02).Shape();gp_Trsf tr;tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),d[3]*pi/180);s=cut(r,BRepBuilderAPI_Transform(gap,tr,true).Shape());break;}
        case S::CylinderLiner:{auto a=ring(d[0],d[1],d[2]);s=fuse(a,ring(d[3],d[1],d[4],d[2]-d[4]));break;}
        case S::CamFollower:{TopoDS_Compound c=compound();addCompound(c,cylinder(d[0],d[1],gp_Pnt(0,0,d[3])));addCompound(c,cylinder(d[2],d[3],gp_Pnt(0,0,0)));addCompound(c,cylinder(d[4],d[2],gp_Pnt(0,0,0)));s=c;break;}
        case S::EccentricCam:{auto cam=cylinder(d[0],d[3]);s=cut(cam,cylinder(d[1],d[3]+0.02,gp_Pnt(d[2],0,-0.01)));break;}
        case S::ChainLink:{TopoDS_Compound c=compound();for(int sign:{-1,1}){addCompound(c,cylinder(d[2],d[1],gp_Pnt(-d[0]/2,sign*d[0]*0.22,0)));addCompound(c,cylinder(d[2],d[1],gp_Pnt(d[0]/2,sign*d[0]*0.22,0)));addCompound(c,box(-d[0]/2,-d[2],0,d[0],2*d[2],d[1]));}addCompound(c,cylinder(d[4],d[1]*0.8,gp_Pnt(-d[0]/2,0,d[1]*0.1),gp_Dir(1,0,0)));addCompound(c,cylinder(d[4],d[1]*0.8,gp_Pnt(d[0]/2,0,d[1]*0.1),gp_Dir(1,0,0)));s=c;break;}
        case S::RollerChainSegment:{TopoDS_Compound c=compound();for(int i=0;i<static_cast<int>(d[4]);++i){gp_Trsf t;t.SetTranslation(gp_Vec(i*d[0],0,0));
                for(int sign:{-1,1}){addCompound(c,BRepBuilderAPI_Transform(cylinder(d[2],d[1],gp_Pnt(-d[0]/2,sign*d[0]*0.22,0)),t,true).Shape());addCompound(c,BRepBuilderAPI_Transform(cylinder(d[2],d[1],gp_Pnt(d[0]/2,sign*d[0]*0.22,0)),t,true).Shape());}
                addCompound(c,BRepBuilderAPI_Transform(cylinder(d[0]*0.2,d[1]*0.8,gp_Pnt(0,0,d[1]*0.1),gp_Dir(1,0,0)),t,true).Shape());}s=c;break;}
        case S::DoubleRowSprocket:{TopoDS_Compound c=compound();auto a=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);addCompound(c,a);gp_Trsf tr;tr.SetTranslation(gp_Vec(0,0,d[2]));addCompound(c,BRepBuilderAPI_Transform(a,tr,true).Shape());s=c;break;}
        case S::FlexibleBellowsCoupling:{auto base=ring(d[1],d[0],d[2]);TopoDS_Compound c=compound();addCompound(c,base);for(int i=0;i<static_cast<int>(d[4]);++i)addCompound(c,ring(d[1],d[1]-d[3],d[2]/(d[4]*2),i*d[2]/d[4]));s=c;break;}
        case S::DiscCoupling:{TopoDS_Compound c=compound();addCompound(c,ring(d[1],d[0],d[2],-d[2]));addCompound(c,ring(d[1],d[0],d[2],d[4]));addCompound(c,ring(d[3],d[0],d[4]));for(int i=0;i<4;++i){double a=pi/4+i*pi/2;addCompound(c,cylinder(d[0]*0.35,d[4],gp_Pnt(d[1]*1.45*cos(a),d[1]*1.45*sin(a),0)));}s=c;break;}
        case S::OldhamCoupling:{TopoDS_Compound c=compound();addCompound(c,ring(d[1],d[0],d[2]));addCompound(c,ring(d[3],d[0],d[4],d[2]));addCompound(c,ring(d[1],d[0],d[2],d[2]+d[4]));s=c;break;}
        case S::YStrainer:{auto run=annularAlongX(d[0],d[1],d[2]);auto branch=cylinder(d[3],d[4],gp_Pnt(0,0,-d[4]),gp_Dir(0.45,0,0.89));auto shell=fuse(run,branch);auto bore=annularAlongX(d[1],0,d[2]+2);s=cut(shell,bore);break;}
        case S::HoseBarb:{TopoDS_Compound c=compound();addCompound(c,annularAlongX(d[0],d[1],d[2]));for(int i=0;i<static_cast<int>(d[4]);++i)addCompound(c,annularAlongX(d[3],d[1],d[0]*0.35,-d[2]/2+d[2]*(i+1)/(d[4]+1)));s=c;break;}
        case S::PipeElbow:{auto a=annularAlongX(d[0],d[1],d[2]);auto b=cylinder(d[0],d[2],gp_Pnt(0,0,0),gp_Dir(0,0,1));auto shell=fuse(a,b);auto bores=fuse(annularAlongX(d[1],0,d[2]+2),cylinder(d[1],d[2]+2,gp_Pnt(0,0,-1)));s=cut(shell,bores);break;}
        case S::PipeCross:{auto a=annularAlongX(d[0],d[1],d[2]);auto b=cylinder(d[0],d[3],gp_Pnt(0,0,-d[3]/2),gp_Dir(0,0,1));auto shell=fuse(a,b);auto bores=fuse(annularAlongX(d[1],0,d[2]+2),cylinder(d[1],d[3]+2,gp_Pnt(0,0,-d[3]/2-1)));s=cut(shell,bores);break;}
        case S::ExpansionJoint:{TopoDS_Compound c=compound();addCompound(c,ring(d[0],d[1],d[2]));for(int i=0;i<static_cast<int>(d[4]);++i)addCompound(c,ring(d[0]+(d[3]-d[0])*0.65,d[1],d[2]/(d[4]*2),i*d[2]/d[4]));addCompound(c,ring(d[3],d[1],d[2]*0.12,-d[2]*0.12));addCompound(c,ring(d[3],d[1],d[2]*0.12,d[2]));s=c;break;}
        case S::CoolingFan:{auto frame=ring(d[0],d[0]*0.88,d[2]);auto hub=cylinder(d[1],d[2]);TopoDS_Shape body=fuse(frame,hub);for(int i=0;i<static_cast<int>(d[3]);++i){auto blade=rotatedBox(d[1]*0.45,-d[4]/2,d[2]*0.15,d[0]-d[1],d[4],d[2]*0.7,2*pi*i/d[3]);body=fuse(body,blade);}s=body;break;}
        case S::Count:return fail("Unknown specialized component.");
        }
        if(s.IsNull()||!BRepCheck_Analyzer(s).IsValid())return fail("Generated specialized component failed topology validation.");
        GProp_GProps props;BRepGProp::VolumeProperties(s,props);
        if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated specialized component has no positive volume.");
        checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),s,true,"#6c9dc2"});return true;
    }catch(const Standard_Failure& e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}
     catch(const std::exception& e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
