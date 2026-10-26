#include "SceneDocument.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cad {
namespace {
using P = EngineeringPart;
constexpr double pi = std::numbers::pi;
constexpr int maxSceneObjects = 2000;

TopoDS_Shape cylinder(double r, double h, const gp_Pnt& o = gp_Pnt(0,0,0),
                      const gp_Dir& axis = gp_Dir(0,0,1)) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(o,axis),r,h).Shape();
}
TopoDS_Shape fuse(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a,b); op.Build();
    if (!op.IsDone() || op.Shape().IsNull()) throw std::runtime_error("OCCT could not fuse component solids.");
    return op.Shape();
}
TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a,b); op.Build();
    if (!op.IsDone() || op.Shape().IsNull()) throw std::runtime_error("OCCT could not cut the component feature.");
    return op.Shape();
}
TopoDS_Shape ring(double outerR, double innerR, double height, double z = 0.0) {
    TopoDS_Shape outer = cylinder(outerR,height,gp_Pnt(0,0,z));
    return innerR > 0.0 ? cut(outer,cylinder(innerR,height,gp_Pnt(0,0,z))) : outer;
}
TopoDS_Shape polygonPrism(const QVector<gp_Pnt>& points, const gp_Vec& direction) {
    BRepBuilderAPI_MakePolygon poly;
    for (const auto& p : points) poly.Add(p);
    poly.Close();
    if (!poly.IsDone()) throw std::runtime_error("Component profile is not a closed polygon.");
    const auto face = BRepBuilderAPI_MakeFace(poly.Wire()).Face();
    return BRepPrimAPI_MakePrism(face,direction).Shape();
}
TopoDS_Shape radialTeeth(double rootR, double tipR, int teeth, double width, double boreR = 0.0) {
    QVector<gp_Pnt> points;
    const double pitch = 2.0*pi/teeth;
    for (int i=0;i<teeth;++i) {
        const double center=i*pitch;
        const double angles[] = {-0.50,-0.25,-0.18,0.18,0.25};
        const double radii[] = {rootR,rootR,tipR,tipR,rootR};
        for (int j=0;j<5;++j) {
            const double a=center+angles[j]*pitch;
            points.append(gp_Pnt(radii[j]*std::cos(a),radii[j]*std::sin(a),0));
        }
    }
    TopoDS_Shape gear=polygonPrism(points,gp_Vec(0,0,width));
    return boreR>0 ? cut(gear,cylinder(boreR,width)) : gear;
}
TopoDS_Shape cutBoltCircle(TopoDS_Shape plate, double circleR, double holeR, int count, double z, double h) {
    for(int i=0;i<count;++i){
        const double a=2.0*pi*i/count;
        plate=cut(plate,cylinder(holeR,h,gp_Pnt(circleR*std::cos(a),circleR*std::sin(a),z)));
    }
    return plate;
}
TopoDS_Wire circleWire(double radius,double z) {
    const gp_Circ circle(gp_Ax2(gp_Pnt(0,0,z),gp_Dir(0,0,1)),radius);
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle).Edge()).Wire();
}
TopoDS_Shape loftCircles(double r0,double r1,double h) {
    BRepOffsetAPI_ThruSections loft(true,true);
    loft.AddWire(circleWire(r0,0)); loft.AddWire(circleWire(r1,h)); loft.Build();
    if(!loft.IsDone()) throw std::runtime_error("Circular reducer loft failed.");
    return loft.Shape();
}
TopoDS_Shape beamProfile(const QVector<gp_Pnt>& profile,double length) {
    return polygonPrism(profile,gp_Vec(0,0,length));
}
TopoDS_Shape hollowBranch(double r,double bore,double length,const gp_Pnt& origin,const gp_Dir& axis) {
    return cut(cylinder(r,length,origin,axis),cylinder(bore,length,origin,axis));
}
bool integerIn(double value,int low,int high) {
    return std::isfinite(value) && value>=low && value<=high && std::floor(value)==value;
}
}

EngineeringPartSpec engineeringPartSpec(EngineeringPart kind) {
    switch(kind) {
    case P::SteppedShaft: return {"Stepped shaft",{"Small radius (mm)","Large radius (mm)","Small section length (mm)","Large section length (mm)"},{6,10,20,30}};
    case P::KeyedShaft: return {"Keyway shaft",{"Radius (mm)","Length (mm)","Key width (mm)","Key depth (mm)"},{10,40,5,3}};
    case P::SplinedShaft: return {"Splined shaft",{"Root radius (mm)","Length (mm)","Tooth height (mm)","Spline count (6-80)"},{8,35,2,12}};
    case P::SolidPulley: return {"Hubbed pulley",{"Rim radius (mm)","Width (mm)","Bore radius (mm)","Hub radius (mm)"},{20,12,4,10}};
    case P::VGroovePulley: return {"V-groove pulley",{"Rim radius (mm)","Width (mm)","Bore radius (mm)","Groove depth (mm)"},{20,14,4,3}};
    case P::MultiGroovePulley: return {"Multi-groove pulley",{"Rim radius (mm)","Width (mm)","Bore radius (mm)","Groove count (2-8)"},{22,20,5,3}};
    case P::FlangedBearing: return {"Flanged bearing",{"Outer radius (mm)","Bore radius (mm)","Body width (mm)","Flange radius (mm)"},{18,8,12,24}};
    case P::BallBearing: return {"Ball bearing assembly",{"Outer radius (mm)","Bore radius (mm)","Width (mm)","Ball radius (mm)"},{20,8,10,2.5}};
    case P::FlangedPipe: return {"Flanged pipe",{"Outer radius (mm)","Inner radius (mm)","Pipe length (mm)","Flange radius (mm)"},{12,8,40,20}};
    case P::BoltCircleFlange: return {"Bolt-circle flange",{"Outer radius (mm)","Bore radius (mm)","Thickness (mm)","Bolt hole radius (mm)","Bolt circle radius (mm)","Hole count (3-16)"},{25,8,6,2,18,6}};
    case P::SquareFlange: return {"Square flange",{"Side (mm)","Bore radius (mm)","Thickness (mm)","Bolt hole radius (mm)","Hole inset (mm)"},{50,8,6,2,7}};
    case P::BlindFlange: return {"Blind flange",{"Outer radius (mm)","Thickness (mm)","Bolt hole radius (mm)","Bolt circle radius (mm)","Hole count (3-16)"},{25,7,2,18,6}};
    case P::PipeElbow: return {"Hollow pipe elbow",{"Centerline radius (mm)","Outer pipe radius (mm)","Wall thickness (mm)","Sweep angle (degrees)"},{28,8,1.5,90}};
    case P::PipeReducer: return {"Hollow pipe reducer",{"Small outer radius (mm)","Large outer radius (mm)","Length (mm)","Wall thickness (mm)"},{8,14,25,1.5}};
    case P::PipeTee: return {"Hollow pipe tee",{"Outer radius (mm)","Inner radius (mm)","Run length (mm)","Branch length (mm)"},{10,7,50,30}};
    case P::PipeYBranch: return {"Hollow Y branch",{"Outer radius (mm)","Inner radius (mm)","Branch length (mm)","Branch angle (degrees)"},{9,6,35,30}};
    case P::PipeCap: return {"Pipe cap",{"Outer radius (mm)","Wall thickness (mm)","Height (mm)","Cap thickness (mm)"},{12,2,15,3}};
    case P::GroovedPipePlug: return {"Grooved pipe plug",{"Stem radius (mm)","Stem length (mm)","Head radius (mm)","Head thickness (mm)"},{7,18,12,6}};
    case P::AngleBracket: return {"Angle bracket",{"Leg length (mm)","Thickness (mm)","Width (mm)","Hole radius (mm)"},{40,5,30,2.5}};
    case P::GussetBracket: return {"Gusset bracket",{"Leg length (mm)","Thickness (mm)","Width (mm)","Gusset height (mm)"},{40,5,30,24}};
    case P::TeeBracket: return {"T bracket",{"Stem width (mm)","Flange width (mm)","Thickness (mm)","Length (mm)"},{12,42,5,30}};
    case P::UChannel: return {"U channel",{"Outer width (mm)","Outer height (mm)","Wall (mm)","Length (mm)"},{32,24,3,50}};
    case P::CChannel: return {"C channel",{"Outer width (mm)","Outer height (mm)","Wall (mm)","Length (mm)"},{36,26,3,50}};
    case P::IBeam: return {"I beam",{"Flange width (mm)","Overall depth (mm)","Web/flange thickness (mm)","Length (mm)"},{30,40,4,60}};
    case P::TBeam: return {"T beam",{"Flange width (mm)","Overall depth (mm)","Web/flange thickness (mm)","Length (mm)"},{36,35,5,55}};
    case P::DovetailRail: return {"Dovetail rail",{"Base width (mm)","Top width (mm)","Height (mm)","Length (mm)"},{30,20,10,60}};
    case P::SpurGear: return {"Spur gear",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Tooth count (6-120)","Bore radius (mm)"},{18,22,8,24,4}};
    case P::RackGear: return {"Rack gear",{"Length (mm)","Face width (mm)","Base height (mm)","Tooth height (mm)","Tooth pitch (mm)"},{100,12,8,4,10}};
    case P::Sprocket: return {"Chain sprocket",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Tooth count (6-80)","Bore radius (mm)"},{18,22,8,18,5}};
    case P::FanImpeller: return {"Fan impeller",{"Outer radius (mm)","Hub radius (mm)","Thickness (mm)","Blade count (3-16)","Blade width (mm)"},{28,8,5,6,8}};
    case P::Count: break;
    }
    return {};
}

bool SceneDocument::addEngineeringPart(EngineeringPart kind,const QVector<double>& d,QString* error) {
    const auto spec=engineeringPartSpec(kind);
    if(spec.name.isEmpty() || d.size()!=spec.defaults.size()) { setError(error,"Unknown component or wrong number of dimensions."); return false; }
    for(double v:d) if(!std::isfinite(v)||v<=1e-5||v>1e6) { setError(error,"Component dimensions must be finite and positive."); return false; }
    if(parts_.size()>=maxSceneObjects){setError(error,"Scene object limit reached.");return false;}
    auto fail=[&](const QString& message){setError(error,message);return false;};
    if((kind==P::SplinedShaft && !integerIn(d[3],6,80)) ||
       (kind==P::MultiGroovePulley && !integerIn(d[3],2,8)) ||
       ((kind==P::BoltCircleFlange||kind==P::BlindFlange) && !integerIn(d.last(),3,16)) ||
       ((kind==P::SpurGear||kind==P::Sprocket) && !integerIn(d[3],6,120)) ||
       (kind==P::FanImpeller && !integerIn(d[3],3,16))) return fail("Instance or tooth counts must be integers within the dialog range.");
    if(kind==P::SteppedShaft && d[1]<=d[0]) return fail("Large shaft radius must exceed small shaft radius.");
    if(kind==P::KeyedShaft && (d[2]>=2*d[0] || d[3]>=d[0])) return fail("Key width and depth must fit within the shaft radius.");
    if((kind==P::SplinedShaft||kind==P::SpurGear||kind==P::Sprocket) && d[0]+d[2]<=d[0]) return fail("Tooth dimensions are invalid.");
    if(kind==P::SolidPulley && (d[2]>=d[0]||d[3]>=d[0])) return fail("Pulley bore and hub must fit inside the rim.");
    if((kind==P::VGroovePulley||kind==P::MultiGroovePulley) && (d[2]>=d[0]||d[3]>=d[0]/2)) return fail("Groove and bore dimensions must fit inside the pulley.");
    if((kind==P::FlangedBearing||kind==P::BallBearing) && (d[1]>=d[0]*0.65||d[2]>=2*d[0])) return fail("Bearing bore or width is outside the supported range.");
    if(kind==P::FlangedBearing && d[3]<=d[0]) return fail("Flange radius must exceed the bearing body radius.");
    if(kind==P::BallBearing && d[3]>=((d[0]-d[1])/4.0)) return fail("Bearing balls must fit between the inner and outer races.");
    if((kind==P::FlangedPipe||kind==P::PipeTee||kind==P::PipeYBranch) && d[1]>=d[0]) return fail("Pipe inner radius must be smaller than its outer radius.");
    if(kind==P::FlangedPipe && d[3]<=d[0]) return fail("Pipe flange radius must exceed the pipe radius.");
    if(kind==P::BoltCircleFlange && (d[1]>=d[0]||d[4]+d[3]>=d[0])) return fail("Bore and bolt circle must fit inside the flange.");
    if(kind==P::SquareFlange && (d[1]>=d[0]/2||d[4]*2>=d[0]||d[4]<=d[3])) return fail("Square flange holes or bore do not fit.");
    if(kind==P::BlindFlange && (d[3]+d[2]>=d[0])) return fail("Bolt circle must fit inside the blind flange.");
    if(kind==P::PipeElbow && (d[0]<=d[1]||d[2]>=d[1]||d[3]>180)) return fail("Elbow centerline, wall, or sweep angle is invalid.");
    if(kind==P::PipeReducer && (d[0]<=d[3]||d[1]<=d[3])) return fail("Reducer wall must leave a positive bore at both ends.");
    if(kind==P::PipeCap && (d[1]>=d[0]||d[3]>=d[2])) return fail("Cap wall and end thickness must fit within the pipe.");
    if(kind==P::GroovedPipePlug && d[2]<=d[0]) return fail("Plug head must exceed stem radius.");
    if((kind==P::UChannel||kind==P::CChannel) && 2*d[2]>=std::min(d[0],d[1])) return fail("Channel walls must fit inside the section.");
    if((kind==P::IBeam||kind==P::TBeam) && 2*d[2]>=std::min(d[0],d[1])) return fail("Beam web and flange thicknesses are too large.");
    if(kind==P::DovetailRail && d[1]>=d[0]) return fail("Dovetail top width must be smaller than the base width.");
    if(kind==P::GussetBracket && d[3]>=d[0]) return fail("Gusset height must be smaller than the bracket legs.");
    if((kind==P::SpurGear||kind==P::Sprocket) && (d[1]<=d[0]||d[4]>=d[0])) return fail("Gear tooth and bore dimensions are invalid.");
    if(kind==P::FanImpeller && (d[1]>=d[0]||d[4]>=d[0]-d[1])) return fail("Impeller hub and blade dimensions do not fit.");
    try {
        TopoDS_Shape shape;
        switch(kind) {
        case P::SteppedShaft: shape=fuse(cylinder(d[0],d[2]),cylinder(d[1],d[3],gp_Pnt(0,0,d[2]))); break;
        case P::KeyedShaft: {
            const auto blank=cylinder(d[0],d[1]);
            shape=cut(blank,BRepPrimAPI_MakeBox(gp_Pnt(d[0]-d[3],-d[2]/2,0),2*d[3],d[2],d[1]).Shape()); break;
        }
        case P::SplinedShaft: shape=radialTeeth(d[0],d[0]+d[2],static_cast<int>(d[3]),d[1]); break;
        case P::SolidPulley: shape=cut(fuse(cylinder(d[0],d[1]),cylinder(d[3],d[1]+d[1]*0.4,gp_Pnt(0,0,-d[1]*0.2))),cylinder(d[2],d[1]*1.4,gp_Pnt(0,0,-d[1]*0.2))); break;
        case P::VGroovePulley: {
            auto blank=cylinder(d[0],d[1]);
            blank=cut(blank,BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0,0,d[1]/2),gp_Dir(0,0,1)),d[0]-d[3]/2,d[3]/2).Shape());
            shape=cut(blank,cylinder(d[2],d[1])); break;
        }
        case P::MultiGroovePulley: {
            auto blank=cylinder(d[0],d[1]); const int n=static_cast<int>(d[3]);
            for(int i=0;i<n;++i){const double z=d[1]*(i+1.0)/(n+1.0);blank=cut(blank,BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0,0,z),gp_Dir(0,0,1)),d[0]-1.0,1.0).Shape());}
            shape=cut(blank,cylinder(d[2],d[1])); break;
        }
        case P::FlangedBearing: {
            auto body=ring(d[0],d[1],d[2]); auto flange=ring(d[3],d[1],d[2]*0.2,-d[2]*0.2); shape=fuse(body,flange); break;
        }
        case P::BallBearing: {
            BRep_Builder b; TopoDS_Compound assembly; b.MakeCompound(assembly);
            b.Add(assembly,ring(d[0],d[0]*0.84,d[2])); b.Add(assembly,ring(d[1]*1.35,d[1],d[2]));
            const double pitch=(d[0]+d[1]*1.65)/2.0; const int n=std::max(6,static_cast<int>(std::floor(pi*pitch/d[3])));
            for(int i=0;i<n;++i){const double a=2*pi*i/n;b.Add(assembly,BRepPrimAPI_MakeSphere(gp_Pnt(pitch*std::cos(a),pitch*std::sin(a),d[2]/2),d[3]).Shape());}
            shape=assembly; break;
        }
        case P::FlangedPipe: shape=fuse(ring(d[0],d[1],d[2]),ring(d[3],d[1],d[2]*0.18,-d[2]*0.18)); break;
        case P::BoltCircleFlange: {
            auto plate=cut(cylinder(d[0],d[2]),cylinder(d[1],d[2]));
            shape=cutBoltCircle(plate,d[4],d[3],static_cast<int>(d[5]),0,d[2]); break;
        }
        case P::SquareFlange: {
            auto plate=BRepPrimAPI_MakeBox(d[0],d[0],d[2]).Shape(); plate=cut(plate,cylinder(d[1],d[2]));
            const double c=d[4]; for(double x:{c,d[0]-c})for(double y:{c,d[0]-c})plate=cut(plate,cylinder(d[3],d[2],gp_Pnt(x,y,0)));
            shape=plate; break;
        }
        case P::BlindFlange: shape=cutBoltCircle(cylinder(d[0],d[1]),d[3],d[2],static_cast<int>(d[4]),0,d[1]); break;
        case P::PipeElbow: {
            const double inner=d[1]-d[2]; const double angle=d[3]*pi/180.0;
            shape=cut(BRepPrimAPI_MakeTorus(d[0],d[1],angle).Shape(),BRepPrimAPI_MakeTorus(d[0],inner,angle).Shape()); break;
        }
        case P::PipeReducer: {
            auto outer=loftCircles(d[1],d[0],d[2]); auto inner=loftCircles(d[1]-d[3],d[0]-d[3],d[2]); shape=cut(outer,inner); break;
        }
        case P::PipeTee: {
            auto run=cylinder(d[0],d[2],gp_Pnt(0,0,-d[2]/2)); auto branch=cylinder(d[0],d[3],gp_Pnt(0,0,0),gp_Dir(1,0,0));
            auto outer=fuse(run,branch); auto runBore=cylinder(d[1],d[2],gp_Pnt(0,0,-d[2]/2)); auto branchBore=cylinder(d[1],d[3],gp_Pnt(0,0,0),gp_Dir(1,0,0));
            shape=cut(outer,fuse(runBore,branchBore)); break;
        }
        case P::PipeYBranch: {
            const double a=d[3]*pi/180.0; auto run=hollowBranch(d[0],d[1],d[2],gp_Pnt(0,0,-d[2]/2),gp_Dir(0,0,1));
            auto left=hollowBranch(d[0],d[1],d[2]*0.7,gp_Pnt(0,0,0),gp_Dir(-std::sin(a),0,std::cos(a)));
            auto right=hollowBranch(d[0],d[1],d[2]*0.7,gp_Pnt(0,0,0),gp_Dir(std::sin(a),0,std::cos(a)));
            shape=fuse(fuse(run,left),right); break;
        }
        case P::PipeCap: {
            auto outer=cylinder(d[0],d[2]); auto cavity=cylinder(d[0]-d[1],d[2]-d[3]); shape=cut(outer,cavity); break;
        }
        case P::GroovedPipePlug: {
            auto stem=cylinder(d[0],d[1]);
            for(int i=0;i<4;++i){double z=d[1]*(i+1.0)/5.0;stem=cut(stem,BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0,0,z),gp_Dir(0,0,1)),d[0]-0.35,0.35).Shape());}
            BRepBuilderAPI_MakePolygon hex; for(int i=0;i<6;++i){double a=2*pi*i/6;hex.Add(gp_Pnt(d[2]*std::cos(a),d[2]*std::sin(a),d[1]));} hex.Close();
            shape=fuse(stem,BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(hex.Wire()).Face(),gp_Vec(0,0,d[3])).Shape()); break;
        }
        case P::AngleBracket: {
            QVector<gp_Pnt> l{{0,0,0},{d[0],0,0},{d[0],d[1],0},{d[1],d[1],0},{d[1],d[0],0},{0,d[0],0}};
            auto bracket=polygonPrism(l,gp_Vec(0,0,d[2]));
            bracket=cut(bracket,cylinder(d[3],d[1],gp_Pnt(d[0]*0.72,d[1]/2,0)));
            bracket=cut(bracket,cylinder(d[3],d[1],gp_Pnt(d[1]/2,d[0]*0.72,0)));
            shape=bracket; break;
        }
        case P::GussetBracket: {
            QVector<gp_Pnt> l{{0,0,0},{d[0],0,0},{d[0],d[1],0},{d[1],d[1],0},{d[1],d[0],0},{0,d[0],0}};
            auto bracket=polygonPrism(l,gp_Vec(0,0,d[2]));
            QVector<gp_Pnt> tri{{0,0,0},{d[3],0,0},{0,d[3],0}};
            shape=fuse(bracket,polygonPrism(tri,gp_Vec(0,0,d[2]))); break;
        }
        case P::TeeBracket: {
            const double x=(d[1]-d[0])/2;
            QVector<gp_Pnt> t{{x,0,0},{x+d[0],0,0},{x+d[0],d[1]-d[2],0},{d[1],d[1]-d[2],0},
                              {d[1],d[1],0},{0,d[1],0},{0,d[1]-d[2],0},{x,d[1]-d[2],0}};
            shape=polygonPrism(t,gp_Vec(0,0,d[3])); break;
        }
        case P::UChannel: {
            QVector<gp_Pnt> p{{0,0,0},{d[0],0,0},{d[0],d[1],0},{d[0]-d[2],d[1],0},{d[0]-d[2],d[2],0},{d[2],d[2],0},{d[2],d[1],0},{0,d[1],0}};
            shape=beamProfile(p,d[3]); break;
        }
        case P::CChannel: {
            QVector<gp_Pnt> p{{0,0,0},{d[0],0,0},{d[0],d[2],0},{d[2],d[2],0},{d[2],d[1]-d[2],0},{d[0],d[1]-d[2],0},{d[0],d[1],0},{0,d[1],0}};
            shape=beamProfile(p,d[3]); break;
        }
        case P::IBeam: {
            const double w=d[0],h=d[1],t=d[2],a=(w-t)/2;
            QVector<gp_Pnt> p{{0,0,0},{w,0,0},{w,t,0},{a+t,t,0},{a+t,h-t,0},{w,h-t,0},{w,h,0},{0,h,0},{0,h-t,0},{a,h-t,0},{a,t,0},{0,t,0}};
            shape=beamProfile(p,d[3]); break;
        }
        case P::TBeam: {
            const double w=d[0],h=d[1],t=d[2],a=(w-t)/2;
            QVector<gp_Pnt> p{{a,0,0},{a+t,0,0},{a+t,h-t,0},{w,h-t,0},{w,h,0},{0,h,0},{0,h-t,0},{a,h-t,0}};
            shape=beamProfile(p,d[3]); break;
        }
        case P::DovetailRail: {
            QVector<gp_Pnt> p{{0,0,0},{d[0],0,0},{(d[0]+d[1])/2,d[2],0},{(d[0]-d[1])/2,d[2],0}};
            shape=beamProfile(p,d[3]); break;
        }
        case P::SpurGear: shape=radialTeeth(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]); break;
        case P::RackGear: {
            auto base=BRepPrimAPI_MakeBox(d[0],d[1],d[2]).Shape(); const int n=static_cast<int>(std::floor(d[0]/d[4]));
            if(n<2||n>200) return fail("Rack length and pitch must produce 2 to 200 teeth.");
            for(int i=0;i<n;++i){const double x=i*d[4]; QVector<gp_Pnt> tooth{{x,0,d[2]},{x+d[4],0,d[2]},{x+d[4]/2,0,d[2]+d[3]}};base=fuse(base,polygonPrism(tooth,gp_Vec(0,d[1],0)));}
            shape=base; break;
        }
        case P::Sprocket: shape=radialTeeth(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]); break;
        case P::FanImpeller: {
            auto impeller=cylinder(d[1],d[2]); const int n=static_cast<int>(d[3]); const double half=d[4]/(2*d[0]);
            for(int i=0;i<n;++i){const double a=2*pi*i/n; QVector<gp_Pnt> blade{
                gp_Pnt(d[1]*std::cos(a-half),d[1]*std::sin(a-half),0),
                gp_Pnt(d[0]*std::cos(a-half),d[0]*std::sin(a-half),0),
                gp_Pnt(d[0]*std::cos(a+half),d[0]*std::sin(a+half),0),
                gp_Pnt(d[1]*std::cos(a+half),d[1]*std::sin(a+half),0)};
                impeller=fuse(impeller,polygonPrism(blade,gp_Vec(0,0,d[2])));
            }
            shape=impeller; break;
        }
        case P::Count: return fail("Unknown engineering component.");
        }
        if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid()) return fail("Generated component failed OCCT topology validation.");
        GProp_GProps props; BRepGProp::VolumeProperties(shape,props);
        if(!std::isfinite(props.Mass())||props.Mass()<=1e-10) return fail("Generated component has no positive volume.");
        checkpoint(); parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),shape});
        return true;
    } catch(const Standard_Failure& e) { return fail(QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch(const std::exception& e) { return fail(QString::fromUtf8(e.what())); }
}
} // namespace cad
