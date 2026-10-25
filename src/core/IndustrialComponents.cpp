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
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <QUuid>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cad {
namespace {
constexpr double pi=std::numbers::pi;
TopoDS_Shape cyl(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir d=gp_Dir(0,0,1)) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(p,d),r,h).Shape();
}
TopoDS_Shape fuseI(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not join industrial component geometry.");
    return op.Shape();
}
TopoDS_Shape cutI(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not cut industrial component geometry.");
    return op.Shape();
}
TopoDS_Shape annulus(double ro,double ri,double h,double z=0) {
    auto a=cyl(ro,h,gp_Pnt(0,0,z));
    return ri>0?cutI(a,cyl(ri,h+0.02,gp_Pnt(0,0,z-0.01))):a;
}
TopoDS_Shape box(double x,double y,double z,double dx,double dy,double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),dx,dy,dz).Shape();
}
TopoDS_Shape radialGear(double root,double tip,int teeth,double width,double bore) {
    BRepBuilderAPI_MakePolygon p;
    for(int i=0;i<teeth;++i){const double a=2*pi*i/teeth,half=pi/(2*teeth);
        p.Add(gp_Pnt(root*cos(a-2*half),root*sin(a-2*half),0));
        p.Add(gp_Pnt(tip*cos(a-half),tip*sin(a-half),0));
        p.Add(gp_Pnt(tip*cos(a+half),tip*sin(a+half),0));
        p.Add(gp_Pnt(root*cos(a+2*half),root*sin(a+2*half),0));}
    p.Close();if(!p.IsDone())throw std::runtime_error("Radial tooth profile is invalid.");
    auto s=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(p.Wire()).Face(),gp_Vec(0,0,width)).Shape();
    return bore>0?cutI(s,cyl(bore,width+0.02,gp_Pnt(0,0,-0.01))):s;
}
TopoDS_Shape rotateZ(const TopoDS_Shape& s,double radians) {
    gp_Trsf t;t.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),radians);
    return BRepBuilderAPI_Transform(s,t,true).Shape();
}
bool countIn(double x,int lo,int hi){return std::isfinite(x)&&x>=lo&&x<=hi&&std::floor(x)==x;}
}

EngineeringPartSpec industrialPartSpec(IndustrialPart kind) {
    using I=IndustrialPart;
    static const QVector<EngineeringPartSpec> specs={
        {"Differential housing",{"Width (mm)","Depth (mm)","Height (mm)","Axle bore radius (mm)","Mount hole radius (mm)"},{88,72,64,18,4},3},
        {"Constant velocity joint",{"Joint radius (mm)","Shaft radius (mm)","Joint length (mm)","Hub radius (mm)","Hub length (mm)"},{22,7,38,13,14},5},
        {"Tripod joint",{"Joint radius (mm)","Shaft radius (mm)","Joint length (mm)","Hub radius (mm)","Hub length (mm)"},{20,7,34,12,12},5},
        {"Torque limiter",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange width (mm)"},{28,8,22,34,5},2},
        {"Centrifugal clutch",{"Drum radius (mm)","Shaft radius (mm)","Body width (mm)","Hub radius (mm)","Hub length (mm)"},{32,8,18,16,16},5},
        {"Multi-disc clutch pack",{"Plate outer radius (mm)","Bore radius (mm)","Pack length (mm)","Flange radius (mm)","Plate thickness (mm)"},{34,10,24,39,2},2},
        {"Flywheel",{"Outer radius (mm)","Bore radius (mm)","Body width (mm)","Flange radius (mm)","Flange thickness (mm)"},{62,12,18,68,5},2},
        {"Flexplate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Bolt-hole radius (mm)","Hole spacing (mm)"},{150,150,4,4,46},8},
        {"Brake caliper",{"Width (mm)","Depth (mm)","Height (mm)","Piston bore radius (mm)","Mount hole radius (mm)"},{86,54,62,14,5},3},
        {"Brake drum",{"Drum outer radius (mm)","Bore radius (mm)","Drum width (mm)","Flange radius (mm)","Flange width (mm)"},{58,24,42,64,6},2},
        {"Clutch pressure plate",{"Outer radius (mm)","Bore radius (mm)","Body width (mm)","Flange radius (mm)","Flange thickness (mm)"},{68,24,15,76,5},2},
        {"Crankshaft pulley",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Groove count (12-60)" ,"Bore radius (mm)"},{28,32,18,24,8},6},
        {"Harmonic balancer",{"Outer radius (mm)","Bore radius (mm)","Body width (mm)","Flange radius (mm)","Flange thickness (mm)"},{48,9,24,54,5},2},
        {"Gearbox housing",{"Width (mm)","Depth (mm)","Height (mm)","Main shaft bore radius (mm)","Mount hole radius (mm)"},{110,82,74,20,5},3},
        {"Angular contact bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{24,12,12,27,3},2},
        {"Double row ball bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{28,14,18,31,3},2},
        {"Thin-section bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{24,20,4,27,2},2},
        {"Cylindrical roller bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{26,14,16,29,3},2},
        {"Spherical plain bearing",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{22,10,14,26,3},2},
        {"Bronze sleeve bushing",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange thickness (mm)"},{14,8,26,19,4},2},
        {"Flanged sleeve bushing",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange thickness (mm)"},{16,9,30,22,5},2},
        {"Bearing locknut",{"Outer radius (mm)","Bore radius (mm)","Thickness (mm)","Flange radius (mm)","Flange thickness (mm)"},{20,12,8,23,3},2},
        {"Bearing retainer plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Center bore radius (mm)","Bolt-hole radius (mm)"},{68,68,6,18,4},8},
        {"Bearing spacer",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange thickness (mm)"},{20,12,8,22,2},2},
        {"Labyrinth seal",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{24,14,12,29,3},2},
        {"Radial shaft seal",{"Outer radius (mm)","Inner radius (mm)","Width (mm)","Flange radius (mm)","Flange thickness (mm)"},{22,12,7,25,2},2},
        {"Oil seal housing",{"Width (mm)","Depth (mm)","Height (mm)","Seal bore radius (mm)","Mount hole radius (mm)"},{58,42,30,14,4},3},
        {"Ball valve body",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18,11,58,14,34},4},
        {"Gate valve body",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20,12,64,16,42},4},
        {"Butterfly valve body",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22,14,42,16,28},4},
        {"Globe valve body",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18,10,54,14,38},4},
        {"Pressure relief valve",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{16,9,48,13,30},4},
        {"Solenoid valve body",{"Width (mm)","Depth (mm)","Height (mm)","Bore radius (mm)","Mount hole radius (mm)"},{64,44,38,10,4},3},
        {"Flow meter housing",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{16,10,52,12,24},4},
        {"Hydraulic manifold block",{"Width (mm)","Depth (mm)","Height (mm)","Port radius (mm)","Mount hole radius (mm)"},{92,68,42,7,5},3},
        {"Weld-neck flange",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange width (mm)"},{20,12,26,38,8},2},
        {"Lap-joint flange",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange width (mm)"},{22,14,18,40,7},2},
        {"Pipe saddle support",{"Width (mm)","Depth (mm)","Height (mm)","Pipe seat radius (mm)","Mount hole radius (mm)"},{52,36,22,14,4},3},
        {"Compression tube elbow",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{9,5,26,8,22},4},
        {"Bulkhead fitting",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange width (mm)"},{12,7,32,18,5},2},
        {"Tank drain fitting",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange width (mm)"},{10,6,24,16,4},2},
        {"Hose reducer",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{15,9,34,8,28},4},
        {"Fire hose nozzle",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{16,10,42,6,22},4},
        {"Centrifugal pump casing",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24,14,54,18,40},4},
        {"Volute pump cover",{"Width (mm)","Depth (mm)","Height (mm)","Center bore radius (mm)","Mount hole radius (mm)"},{74,64,18,18,4},3},
        {"Plate heat exchanger",{"Width (mm)","Depth (mm)","Base thickness (mm)","Fin thickness (mm)","Plate count (4-24)"},{80,48,8,2,12},9},
        {"Shell-and-tube exchanger",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{26,20,88,34,7},2},
        {"Radiator core",{"Width (mm)","Depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{100,52,5,1.5,18},9},
        {"Expansion tank",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{34,30,72,38,6},2},
        {"Fluid reservoir",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{38,34,82,43,6},2},
        {"Pressure vessel shell",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{32,27,90,38,6},2},
        {"Elliptical vessel head",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{30,24,18,34,5},2},
        {"Sight glass assembly",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{18,12,26,24,5},2},
        {"Filter canister",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{28,23,64,34,6},2},
        {"Oil filter housing",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{25,20,58,31,6},2},
        {"Intake manifold",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18,12,68,8,34},4},
        {"Exhaust manifold",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20,14,74,9,38},4},
        {"T-slot extrusion",{"Width (mm)","Depth (mm)","Length (mm)","Slot radius (mm)","Mount bore radius (mm)"},{40,40,120,5,4},8},
        {"Machine tooling plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Hole radius (mm)","Hole spacing (mm)"},{160,120,12,4,24},8},
        {"Electronics enclosure",{"Width (mm)","Depth (mm)","Height (mm)","Cable port radius (mm)","Mount hole radius (mm)"},{120,84,46,8,4},3},
        {"Motor frame",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{42,34,78,50,8},2},
        {"Motor end bell",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{40,12,18,48,7},2},
        {"Stator core",{"Outer radius (mm)","Inner radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{36,24,48,40,4},2},
        {"Rotor core",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{23,8,50,28,4},2},
        {"Fan shroud",{"Frame radius (mm)","Hub radius (mm)","Thickness (mm)","Blade count (4-16)","Blade width (mm)"},{48,10,12,8,9},7},
        {"Cable gland",{"Outer radius (mm)","Bore radius (mm)","Length (mm)","Flange radius (mm)","Flange width (mm)"},{14,8,24,19,5},2},
        {"Straight cable tray",{"Width (mm)","Depth (mm)","Length (mm)","Side height (mm)","Wall thickness (mm)"},{100,50,120,24,3},8},
        {"Perforated cable tray",{"Width (mm)","Depth (mm)","Length (mm)","Side height (mm)","Wall thickness (mm)"},{100,50,120,24,3},8},
        {"Pipe shoe support",{"Width (mm)","Depth (mm)","Height (mm)","Pipe seat radius (mm)","Mount hole radius (mm)"},{66,52,28,20,5},3},
        {"Pipe hanger",{"Width (mm)","Depth (mm)","Height (mm)","Pipe seat radius (mm)","Mount hole radius (mm)"},{56,44,32,16,4},3}
    };
    const int index=static_cast<int>(kind);
    if(index<0||index>=specs.size())return {};
    return specs[index];
}

bool SceneDocument::addIndustrialPart(IndustrialPart kind,const QVector<double>& d,QString* error) {
    using I=IndustrialPart;
    const auto spec=industrialPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=spec.defaults.size()){setError(error,"Unknown industrial component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Industrial component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString& m){setError(error,m);return false;};
    if(spec.geometryFamily==6&&!countIn(d[3],12,60))return fail("Tooth count must be a whole number from 12 through 60.");
    if(spec.geometryFamily==7&&!countIn(d[3],4,16))return fail("Blade count must be a whole number from 4 through 16.");
    if(spec.geometryFamily==9&&!countIn(d[4],4,24))return fail("Fin count must be a whole number from 4 through 24.");
    if(spec.geometryFamily==2&&(d[1]>=d[0]||d[3]<=d[0]||d[4]>=d[2]))return fail("Bore, flange and length dimensions do not fit.");
    const bool saddleSupport=kind==I::PipeSaddleSupport||kind==I::PipeShoeSupport||kind==I::PipeHanger;
    const bool portedCover=kind==I::VolutePumpCover;
    if(spec.geometryFamily==3&&((!saddleSupport&&!portedCover&&d[3]>=std::min(d[1],d[2])/2)||d[4]>=std::min(d[0],d[1])/4))return fail("Bore and mounting holes do not fit the block.");
    if(spec.geometryFamily==4&&(d[1]>=d[0]||d[3]>=d[0]||d[4]<=d[0]))return fail("Branch and bore dimensions do not fit the pipe body.");
    if(spec.geometryFamily==5&&(d[1]>=d[0]||d[3]<=d[1]||d[3]>=d[0]*1.5))return fail("Joint shaft and hub dimensions do not fit.");
    if(spec.geometryFamily==6&&(d[0]>=d[1]||d[4]>=d[0]))return fail("Tooth root, tip and bore radii do not fit.");
    if(spec.geometryFamily==7&&(d[1]>=d[0]*0.6||d[4]>=d[0]-d[1]))return fail("Fan hub and blade dimensions do not fit the frame.");
    if(spec.geometryFamily==8&&(d[3]*2>=std::min(d[0],d[1])||d[4]>=std::min(d[0],d[1])/2))return fail("Plate holes or spacing do not fit the plate.");
    if(spec.geometryFamily==9&&(d[3]*d[4]>=d[0]-2||d[4]<=2))return fail("Fins do not fit the base width.");
    if(kind==I::PerforatedCableTray&&d[4]*2>=std::min(d[0],d[1]))return fail("Tray wall thickness is too large.");
    try {
        TopoDS_Shape s;
        switch(spec.geometryFamily){
        case 1:{auto shaft=cyl(d[0],d[1]);shaft=fuseI(shaft,cyl(d[2],d[4],gp_Pnt(0,0,d[1]-d[4]*0.9)));if(d[3]>0)shaft=cutI(shaft,cyl(d[3],d[1]+0.02,gp_Pnt(0,0,-0.01)));s=shaft;break;}
        case 2:{auto body=annulus(d[0],d[1],d[2]);s=fuseI(body,annulus(d[3],d[1],d[4],d[2]-d[4]));break;}
        case 3:{auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);if(saddleSupport)body=cutI(body,cyl(d[3],d[0]+2,gp_Pnt(-d[0]/2-1,0,d[2]+d[3]*0.7),gp_Dir(1,0,0)));else if(portedCover)body=cutI(body,cyl(d[3],d[2]+2,gp_Pnt(0,0,-1)));else body=cutI(body,cyl(d[3],d[0]+2,gp_Pnt(-d[0]/2-1,0,d[2]/2),gp_Dir(1,0,0)));for(double x:{-d[0]*0.32,d[0]*0.32})for(double y:{-d[1]*0.32,d[1]*0.32})body=cutI(body,cyl(d[4],d[2]+2,gp_Pnt(x,y,-1)));s=body;break;}
        case 4:{auto outer=cyl(d[0],d[2],gp_Pnt(0,0,-d[2]/2));outer=fuseI(outer,cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0)));auto bores=fuseI(cyl(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1)),cyl(d[1]*0.8,d[4]+2,gp_Pnt(0,0,0),gp_Dir(1,0,0)));s=cutI(outer,bores);break;}
        case 5:{auto body=cyl(d[0],d[2]);body=fuseI(body,cyl(d[3],d[4],gp_Pnt(0,0,d[2]-d[4]*0.65)));if(d[1]>0)body=cutI(body,cyl(d[1],d[2]+d[4]+2,gp_Pnt(0,0,-1)));s=body;break;}
        case 6:s=radialGear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case 7:{auto body=annulus(d[0],d[0]*0.88,d[2]);body=fuseI(body,cyl(d[1],d[2]));auto blade=box(d[1]*0.7,-d[4]/2,d[2]*0.15,d[0]-d[1]*0.7,d[4],d[2]*0.7);for(int i=0;i<static_cast<int>(d[3]);++i)body=fuseI(body,rotateZ(blade,2*pi*i/d[3]));s=body;break;}
        case 8:{auto plate=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);const double px=d[4]/2,py=d[4]/2;for(double x:{-px,px})for(double y:{-py,py})plate=cutI(plate,cyl(d[3],d[2]+2,gp_Pnt(x,y,-1)));s=plate;break;}
        case 9:{auto base=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);const double gap=(d[0]-d[4]*d[3])/(d[4]+1);for(int i=0;i<static_cast<int>(d[4]);++i){double x=-d[0]/2+gap+(gap+d[3])*i;base=fuseI(base,box(x,-d[1]/2,d[2],d[3],d[1],d[2]));}s=base;break;}
        default:return fail("Industrial component has an unsupported geometry family.");
        }
        if(s.IsNull()||!BRepCheck_Analyzer(s).IsValid())return fail("Generated industrial component failed topology validation.");
        GProp_GProps props;BRepGProp::VolumeProperties(s,props);
        if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated industrial component has no positive volume.");
        checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),s,true,"#6c9dc2"});return true;
    }catch(const Standard_Failure& e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}
     catch(const std::exception& e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
