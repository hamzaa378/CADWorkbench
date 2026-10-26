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

namespace cad {
namespace {
constexpr double pi=std::numbers::pi;
TopoDS_Shape cyl(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir d=gp_Dir(0,0,1)) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(p,d),r,h).Shape();
}
TopoDS_Shape fuse(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not join extended component geometry.");
    return op.Shape();
}
TopoDS_Shape cut(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a,b);op.Build();
    if(!op.IsDone()||op.Shape().IsNull())throw std::runtime_error("Could not cut extended component geometry.");
    return op.Shape();
}
TopoDS_Shape annulus(double ro,double ri,double h,double z=0) {
    const auto outer=cyl(ro,h,gp_Pnt(0,0,z));
    return ri>0?cut(outer,cyl(ri,h+0.02,gp_Pnt(0,0,z-0.01))):outer;
}
TopoDS_Shape box(double x,double y,double z,double dx,double dy,double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),dx,dy,dz).Shape();
}
TopoDS_Shape gear(double root,double tip,int teeth,double width,double bore) {
    BRepBuilderAPI_MakePolygon wire;
    for(int i=0;i<teeth;++i) {
        const double a=2*pi*i/teeth,h=pi/(2*teeth);
        wire.Add(gp_Pnt(root*std::cos(a-2*h),root*std::sin(a-2*h),0));
        wire.Add(gp_Pnt(tip*std::cos(a-h),tip*std::sin(a-h),0));
        wire.Add(gp_Pnt(tip*std::cos(a+h),tip*std::sin(a+h),0));
        wire.Add(gp_Pnt(root*std::cos(a+2*h),root*std::sin(a+2*h),0));
    }
    wire.Close();
    if(!wire.IsDone())throw std::runtime_error("Gear profile is invalid.");
    auto shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,0,width)).Shape();
    return bore>0?cut(shape,cyl(bore,width+0.02,gp_Pnt(0,0,-0.01))):shape;
}
TopoDS_Shape rotateZ(const TopoDS_Shape& shape,double angle) {
    gp_Trsf transform;transform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),angle);
    return BRepBuilderAPI_Transform(shape,transform,true).Shape();
}
bool wholeIn(double v,int lo,int hi) { return std::isfinite(v)&&v>=lo&&v<=hi&&std::floor(v)==v; }
}

EngineeringPartSpec extendedPartSpec(ExtendedPart kind) {
    static const QVector<EngineeringPartSpec> specs={
        {"Conveyor drive drum",{"Outer radius (mm)","Bore radius (mm)","Drum length (mm)","End flange radius (mm)","Flange thickness (mm)"},{82,14,180,96,8},2},
        {"Conveyor bend pulley",{"Pulley radius (mm)","Shaft radius (mm)","Face width (mm)","Hub radius (mm)","Hub length (mm)"},{68,12,90,24,18},5},
        {"Belt take-up screw",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{12,160,18,4,14},1},
        {"Belt tracking roller",{"Outer radius (mm)","Bore radius (mm)","Roller length (mm)","End flange radius (mm)","Flange thickness (mm)"},{32,8,110,40,6},2},
        {"Bucket elevator head pulley",{"Outer radius (mm)","Bore radius (mm)","Pulley width (mm)","End flange radius (mm)","Flange thickness (mm)"},{105,18,140,120,10},2},
        {"Screw conveyor end plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Shaft opening radius (mm)","Bolt-hole spacing (mm)"},{150,120,12,24,70},8},
        {"Roller conveyor side frame",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mount-hole radius (mm)","Hole spacing (mm)"},{180,90,10,5,50},8},
        {"Pallet rack foot plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Anchor-hole radius (mm)","Hole spacing (mm)"},{140,120,14,8,65},8},
        {"Hoist cable drum",{"Outer radius (mm)","Bore radius (mm)","Drum length (mm)","End flange radius (mm)","Flange thickness (mm)"},{76,16,125,92,12},2},
        {"Crane trolley wheel",{"Hub radius (mm)","Shaft radius (mm)","Wheel width (mm)","Flange radius (mm)","Hub length (mm)"},{58,18,52,76,16},5},
        {"Lathe chuck backplate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Center bore radius (mm)","Bolt-circle spacing (mm)"},{180,180,20,32,100},8},
        {"Rotary table faceplate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Center bore radius (mm)","T-slot spacing (mm)"},{220,220,22,20,120},8},
        {"Vise fixed jaw",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mount-hole radius (mm)","Hole spacing (mm)"},{130,46,24,6,30},8},
        {"Vise swivel base",{"Width (mm)","Depth (mm)","Base thickness (mm)","Center bore radius (mm)","Mount-hole spacing (mm)"},{160,140,24,18,80},8},
        {"Spindle bearing cartridge",{"Outer radius (mm)","Bore radius (mm)","Cartridge length (mm)","Flange radius (mm)","Flange thickness (mm)"},{44,24,72,58,12},2},
        {"Tailstock quill",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{22,180,30,8,20},1},
        {"Collet chuck",{"Shaft radius (mm)","Body length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{26,92,34,12,18},1},
        {"Tool setter block",{"Width (mm)","Depth (mm)","Height (mm)","Bore radius (mm)","Mount-hole radius (mm)"},{64,58,48,8,4},3},
        {"Press ram",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{34,140,46,10,22},1},
        {"Machine leveling foot",{"Outer radius (mm)","Bore radius (mm)","Foot length (mm)","Flange radius (mm)","Flange thickness (mm)"},{32,8,42,44,8},2},
        {"Scroll compressor shell",{"Outer radius (mm)","Inner radius (mm)","Shell length (mm)","Flange radius (mm)","Flange thickness (mm)"},{82,72,140,94,10},2},
        {"Hermetic compressor cover",{"Outer radius (mm)","Inner radius (mm)","Cover depth (mm)","Flange radius (mm)","Flange thickness (mm)"},{76,66,50,88,8},2},
        {"Condenser header",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{24,17,160,12,72},4},
        {"Evaporator distributor",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{18,12,110,8,64},4},
        {"Refrigerant filter drier",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","End-cap radius (mm)","End-cap thickness (mm)"},{32,25,130,38,10},2},
        {"Expansion valve body",{"Width (mm)","Depth (mm)","Height (mm)","Port radius (mm)","Mount-hole radius (mm)"},{72,58,48,12,4},3},
        {"Blower scroll housing",{"Frame radius (mm)","Hub radius (mm)","Thickness (mm)","Blade count (4-16)","Blade width (mm)"},{55,14,14,10,10},7},
        {"Duct transition",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{38,32,200,22,105},4},
        {"Duct access panel",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Fastener-hole radius (mm)","Hole spacing (mm)"},{180,130,8,4,82},8},
        {"Heat pump accumulator",{"Outer radius (mm)","Inner radius (mm)","Shell length (mm)","Flange radius (mm)","Flange thickness (mm)"},{54,46,120,64,9},2},
        {"Tractor axle hub",{"Hub radius (mm)","Shaft radius (mm)","Hub length (mm)","Flange radius (mm)","Flange length (mm)"},{68,22,72,88,16},5},
        {"Implement hitch plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Pin-hole radius (mm)","Hole spacing (mm)"},{180,120,20,18,76},8},
        {"Drawbar clevis",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Pin-hole radius (mm)","Hole spacing (mm)"},{150,88,18,16,52},8},
        {"Plow share",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Fastener-hole radius (mm)","Hole spacing (mm)"},{190,110,12,7,52},8},
        {"Cultivator tine clamp",{"Width (mm)","Depth (mm)","Height (mm)","Clamp bore radius (mm)","Mount-hole radius (mm)"},{82,66,54,12,5},3},
        {"Seed metering housing",{"Width (mm)","Depth (mm)","Height (mm)","Meter bore radius (mm)","Mount-hole radius (mm)"},{100,82,72,18,5},3},
        {"Irrigation sprinkler head",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{20,13,48,12,36},4},
        {"Drip line manifold",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{22,15,170,7,62},4},
        {"Harvester pulley",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Tooth count (12-60)","Bore radius (mm)"},{38,44,26,24,10},6},
        {"Mower blade carrier",{"Hub radius (mm)","Shaft radius (mm)","Carrier length (mm)","Flange radius (mm)","Flange length (mm)"},{28,10,22,38,8},5},
        {"Propeller shaft stern tube",{"Outer radius (mm)","Bore radius (mm)","Tube length (mm)","Flange radius (mm)","Flange thickness (mm)"},{34,24,220,46,12},2},
        {"Rudder pintle",{"Shaft radius (mm)","Shaft length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{20,150,28,6,18},1},
        {"Steering quadrant",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Center bore radius (mm)","Mount-hole spacing (mm)"},{180,150,22,24,82},8},
        {"Seacock body",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{28,18,92,16,62},4},
        {"Through-hull fitting",{"Outer radius (mm)","Bore radius (mm)","Tube length (mm)","Flange radius (mm)","Flange thickness (mm)"},{26,17,72,42,10},2},
        {"Deck cleat base",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mount-hole radius (mm)","Hole spacing (mm)"},{150,88,18,6,70},8},
        {"Anchor chain stopper",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Pin-hole radius (mm)","Hole spacing (mm)"},{130,110,24,12,60},8},
        {"Bilge pump strainer",{"Outer radius (mm)","Bore radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{48,38,64,62,10},2},
        {"Tank inspection hatch",{"Outer radius (mm)","Inner radius (mm)","Hatch depth (mm)","Flange radius (mm)","Flange thickness (mm)"},{72,58,28,90,8},2},
        {"Mast step plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mast socket radius (mm)","Bolt spacing (mm)"},{170,150,24,32,76},8},
        {"Busbar support insulator",{"Width (mm)","Depth (mm)","Height (mm)","Cable bore radius (mm)","Mount-hole radius (mm)"},{72,54,66,10,5},3},
        {"Cable ladder rung",{"Shaft radius (mm)","Rung length (mm)","Shoulder radius (mm)","Bore radius (mm)","Shoulder length (mm)"},{8,420,12,2,10},1},
        {"Cable tray splice plate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Fastener-hole radius (mm)","Hole spacing (mm)"},{210,60,8,4,48},8},
        {"Junction box bracket",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mount-hole radius (mm)","Hole spacing (mm)"},{100,74,10,5,48},8},
        {"DIN rail end stop",{"Width (mm)","Depth (mm)","Height (mm)","Clamp bore radius (mm)","Mount-hole radius (mm)"},{42,32,38,7,3},3},
        {"Transformer mounting foot",{"Width (mm)","Depth (mm)","Height (mm)","Cable bore radius (mm)","Mount-hole radius (mm)"},{92,72,52,10,5},3},
        {"Grounding lug",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Cable-hole radius (mm)","Hole spacing (mm)"},{86,48,12,9,28},8},
        {"Enclosure door hinge",{"Hub radius (mm)","Shaft radius (mm)","Hinge length (mm)","Flange radius (mm)","Flange length (mm)"},{18,6,82,25,12},5},
        {"Control panel backplate",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Mount-hole radius (mm)","Hole spacing (mm)"},{300,220,3,5,120},8},
        {"Power electronics heat sink",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin count (4-24)"},{160,100,8,2,20},9},
        {"Cyclone separator body",{"Outer radius (mm)","Inner radius (mm)","Body length (mm)","Flange radius (mm)","Flange thickness (mm)"},{90,80,160,104,12},2},
        {"Dust collector hopper",{"Width (mm)","Depth (mm)","Height (mm)","Outlet radius (mm)","Mount-hole radius (mm)"},{140,120,88,18,5},3},
        {"Bag filter tube sheet",{"Width (mm)","Depth (mm)","Plate thickness (mm)","Tube-hole radius (mm)","Hole spacing (mm)"},{260,220,12,14,82},8},
        {"Rotary airlock housing",{"Outer radius (mm)","Inner radius (mm)","Housing length (mm)","Flange radius (mm)","Flange thickness (mm)"},{74,62,94,86,10},2},
        {"Slurry pump casing",{"Pipe outer radius (mm)","Pipe bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch length (mm)"},{42,30,120,28,88},4},
        {"Gear pump cover",{"Outer radius (mm)","Inner radius (mm)","Cover length (mm)","Flange radius (mm)","Flange thickness (mm)"},{64,48,34,76,8},2},
        {"Hydraulic accumulator shell",{"Outer radius (mm)","Inner radius (mm)","Shell length (mm)","Flange radius (mm)","Flange thickness (mm)"},{54,45,210,64,10},2},
        {"Pneumatic manifold block",{"Width (mm)","Depth (mm)","Height (mm)","Port radius (mm)","Mount-hole radius (mm)"},{140,72,52,8,5},3},
        {"Quick coupler body",{"Hub radius (mm)","Shaft radius (mm)","Body length (mm)","Flange radius (mm)","Flange length (mm)"},{26,12,58,34,10},5},
        {"Sampling port housing",{"Width (mm)","Depth (mm)","Height (mm)","Port radius (mm)","Mount-hole radius (mm)"},{58,54,46,8,4},3}
    };
    const int index=static_cast<int>(kind);
    if(index<0||index>=specs.size())return {};
    return specs[index];
}

bool SceneDocument::addExtendedPart(ExtendedPart kind,const QVector<double>& d,QString* error) {
    const auto spec=extendedPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=spec.defaults.size()){setError(error,"Unknown extended component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Extended component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString& message){setError(error,message);return false;};
    if(spec.geometryFamily==1&&(d[2]<=d[0]||d[3]>=d[0]||d[4]>=d[1]))return fail("Shoulder, bore and shaft lengths do not fit.");
    if(spec.geometryFamily==2&&(d[1]>=d[0]||d[3]<=d[0]||d[4]>=d[2]))return fail("Bore, flange and body dimensions do not fit.");
    if(spec.geometryFamily==3&&(d[3]>=std::min(d[1],d[2])/2||d[4]>=std::min(d[0],d[1])/4))return fail("Port and mounting holes do not fit the block.");
    if(spec.geometryFamily==4&&(d[1]>=d[0]||d[3]>=d[0]||d[4]<=d[0]))return fail("Pipe bore and branch dimensions do not fit.");
    if(spec.geometryFamily==5&&(d[1]>=d[0]||d[3]<=d[1]||d[3]>=d[0]*1.5))return fail("Shaft and wheel hub dimensions do not fit.");
    if(spec.geometryFamily==6&&(!wholeIn(d[3],12,60)||d[0]>=d[1]||d[4]>=d[0]))return fail("Gear tooth count or radii do not fit.");
    if(spec.geometryFamily==7&&(!wholeIn(d[3],4,16)||d[1]>=d[0]*0.6||d[4]>=d[0]-d[1]))return fail("Fan blade count or dimensions do not fit.");
    if(spec.geometryFamily==8&&(d[3]*2>=std::min(d[0],d[1])||d[4]+2*d[3]>=std::min(d[0],d[1])))return fail("Plate holes or spacing do not fit.");
    if(spec.geometryFamily==9&&(!wholeIn(d[4],4,24)||d[3]*d[4]>=d[0]-2))return fail("Fin count or thickness does not fit the base.");
    try {
        TopoDS_Shape shape;
        switch(spec.geometryFamily) {
        case 1: {
            auto shaft=cyl(d[0],d[1]);
            shaft=fuse(shaft,cyl(d[2],d[4],gp_Pnt(0,0,d[1]-d[4]*0.9)));
            shape=d[3]>0?cut(shaft,cyl(d[3],d[1]+0.02,gp_Pnt(0,0,-0.01))):shaft;
            break;
        }
        case 2: {
            auto body=annulus(d[0],d[1],d[2]);
            shape=fuse(body,annulus(d[3],d[1],d[4],d[2]-d[4]));
            break;
        }
        case 3: {
            auto body=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);
            body=cut(body,cyl(d[3],d[0]+2,gp_Pnt(-d[0]/2-1,0,d[2]/2),gp_Dir(1,0,0)));
            for(double x:{-d[0]*0.32,d[0]*0.32})for(double y:{-d[1]*0.32,d[1]*0.32})
                body=cut(body,cyl(d[4],d[2]+2,gp_Pnt(x,y,-1)));
            shape=body;break;
        }
        case 4: {
            auto outer=cyl(d[0],d[2],gp_Pnt(0,0,-d[2]/2));
            outer=fuse(outer,cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0)));
            auto bores=fuse(cyl(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1)),cyl(d[1]*0.8,d[4]+2,gp_Pnt(0,0,0),gp_Dir(1,0,0)));
            shape=cut(outer,bores);break;
        }
        case 5: {
            auto body=cyl(d[0],d[2]);
            body=fuse(body,cyl(d[3],d[4],gp_Pnt(0,0,d[2]-d[4]*0.65)));
            shape=cut(body,cyl(d[1],d[2]+d[4]+2,gp_Pnt(0,0,-1)));break;
        }
        case 6: shape=gear(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case 7: {
            auto body=annulus(d[0],d[0]*0.88,d[2]);
            body=fuse(body,cyl(d[1],d[2]));
            auto blade=box(d[1]*0.7,-d[4]/2,d[2]*0.15,d[0]-d[1]*0.7,d[4],d[2]*0.7);
            for(int i=0;i<static_cast<int>(d[3]);++i)body=fuse(body,rotateZ(blade,2*pi*i/d[3]));
            shape=body;break;
        }
        case 8: {
            auto plate=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);
            const double p=d[4]/2;
            for(double x:{-p,p})for(double y:{-p,p})plate=cut(plate,cyl(d[3],d[2]+2,gp_Pnt(x,y,-1)));
            shape=plate;break;
        }
        case 9: {
            auto base=box(-d[0]/2,-d[1]/2,0,d[0],d[1],d[2]);
            const double gap=(d[0]-d[4]*d[3])/(d[4]+1);
            for(int i=0;i<static_cast<int>(d[4]);++i) {
                const double x=-d[0]/2+gap+(gap+d[3])*i;
                base=fuse(base,box(x,-d[1]/2,d[2],d[3],d[1],d[2]));
            }
            shape=base;break;
        }
        default:return fail("Extended component has an unsupported geometry family.");
        }
        if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid())return fail("Generated extended component failed topology validation.");
        GProp_GProps properties;BRepGProp::VolumeProperties(shape,properties);
        if(!std::isfinite(properties.Mass())||properties.Mass()<=1e-10)return fail("Generated extended component has no positive volume.");
        checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),shape,true,"#709b79"});return true;
    } catch(const Standard_Failure& e) {
        return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));
    } catch(const std::exception& e) {
        return fail(QString::fromUtf8(e.what()));
    }
}
} // namespace cad
