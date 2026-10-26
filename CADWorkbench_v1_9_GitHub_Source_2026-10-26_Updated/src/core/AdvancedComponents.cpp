#include "SceneDocument.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax1.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GProp_GProps.hxx>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cad {
namespace {
using A = AdvancedPart;
constexpr double pi = std::numbers::pi;
TopoDS_Shape cyl(double r,double h,gp_Pnt p=gp_Pnt(0,0,0),gp_Dir a=gp_Dir(0,0,1)) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(p,a),r,h).Shape();
}
TopoDS_Shape fuseA(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a,b); op.Build();
    if(!op.IsDone()||op.Shape().IsNull()) throw std::runtime_error("Could not fuse advanced component features.");
    return op.Shape();
}
TopoDS_Shape cutA(const TopoDS_Shape& a,const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a,b); op.Build();
    if(!op.IsDone()||op.Shape().IsNull()) throw std::runtime_error("Could not cut advanced component feature.");
    return op.Shape();
}
TopoDS_Shape annulus(double ro,double ri,double h,double z=0) {
    auto out=cyl(ro,h,gp_Pnt(0,0,z));
    return ri>0?cutA(out,cyl(ri,h,gp_Pnt(0,0,z))):out;
}
TopoDS_Shape polyPrism(const QVector<gp_Pnt>& pts,const gp_Vec& v) {
    BRepBuilderAPI_MakePolygon p; for(const auto& q:pts)p.Add(q); p.Close();
    if(!p.IsDone())throw std::runtime_error("Advanced component profile is invalid.");
    return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(p.Wire()).Face(),v).Shape();
}
TopoDS_Shape boltHoles(TopoDS_Shape body,double pitch,double radius,int count,double z,double h) {
    for(int i=0;i<count;++i){const double t=2*pi*i/count;body=cutA(body,cyl(radius,h,gp_Pnt(pitch*cos(t),pitch*sin(t),z)));}
    return body;
}
TopoDS_Shape radialToothedBelt(double inner,double outer,int teeth,double width,double bore) {
    QVector<gp_Pnt> pts;
    for(int i=0;i<teeth;++i){const double a=2*pi*i/teeth;const double half=pi/teeth;
        pts<<gp_Pnt(inner*cos(a-half),inner*sin(a-half),0)
           <<gp_Pnt(outer*cos(a-half*0.5),outer*sin(a-half*0.5),0)
           <<gp_Pnt(outer*cos(a+half*0.5),outer*sin(a+half*0.5),0)
           <<gp_Pnt(inner*cos(a+half),inner*sin(a+half),0);
    }
    auto s=polyPrism(pts,gp_Vec(0,0,width));
    return bore>0?cutA(s,cyl(bore,width)):s;
}
bool isCount(double v,int lo,int hi){return std::isfinite(v)&&v>=lo&&v<=hi&&std::floor(v)==v;}
}

EngineeringPartSpec advancedPartSpec(AdvancedPart k) {
    switch(k){
    case A::HexBolt:return {"Hex bolt",{"Shank radius (mm)","Shank length (mm)","Head radius (mm)","Head height (mm)","Head flats (mm)"},{4,30,7,5,12}};
    case A::SocketHeadScrew:return {"Socket head screw",{"Shank radius (mm)","Shank length (mm)","Head radius (mm)","Head height (mm)","Socket radius (mm)"},{4,25,6.5,6,2.5}};
    case A::FlatWasher:return {"Flat washer",{"Outer radius (mm)","Inner radius (mm)","Thickness (mm)"},{10,4.2,2}};
    case A::ToothLockWasher:return {"Tooth lock washer",{"Outer radius (mm)","Inner radius (mm)","Thickness (mm)","Tooth length (mm)","Tooth count (4-16)"},{10,4.2,1.5,2,8}};
    case A::DowelPin:return {"Dowel pin",{"Radius (mm)","Length (mm)","End chamfer length (mm)"},{4,30,1.5}};
    case A::ClevisPin:return {"Clevis pin",{"Shank radius (mm)","Shank length (mm)","Head radius (mm)","Head thickness (mm)","Cross-hole radius (mm)"},{4,30,7,3,1.5}};
    case A::TaperPin:return {"Taper pin",{"Small radius (mm)","Large radius (mm)","Length (mm)"},{3,4,30}};
    case A::ShaftCollar:return {"Shaft collar",{"Outer radius (mm)","Bore radius (mm)","Width (mm)","Clamp-screw radius (mm)"},{12,6,10,2}};
    case A::FlangeCoupling:return {"Flange coupling",{"Shaft radius (mm)","Hub radius (mm)","Hub length (mm)","Flange radius (mm)","Flange thickness (mm)","Bolt-hole radius (mm)","Bolt circle radius (mm)","Bolt count (4-12)"},{5,12,18,25,6,2,19,6}};
    case A::JawCoupling:return {"Jaw coupling",{"Shaft radius (mm)","Hub radius (mm)","Hub length (mm)","Jaw radius (mm)","Jaw count (3-8)"},{5,12,18,4,6}};
    case A::PillowBlock:return {"Pillow block",{"Shaft radius (mm)","Bearing radius (mm)","Bearing length (mm)","Base width (mm)","Base height (mm)"},{6,15,18,48,12}};
    case A::SplitBearingHousing:return {"Split bearing housing",{"Shaft radius (mm)","Body width (mm)","Body height (mm)","Length (mm)","Mount hole radius (mm)"},{6,42,32,36,3}};
    case A::LinearGuideRail:return {"Linear guide rail",{"Base width (mm)","Base thickness (mm)","Rail top width (mm)","Rail height (mm)","Length (mm)","Mount hole radius (mm)"},{30,6,16,8,100,2.5}};
    case A::LinearBearingBlock:return {"Linear bearing block",{"Block width (mm)","Block height (mm)","Block length (mm)","Bore radius (mm)","Mount hole radius (mm)"},{40,28,32,8,3}};
    case A::BallScrewSupport:return {"Ball screw support",{"Base width (mm)","Base height (mm)","Length (mm)","Screw radius (mm)","Bearing radius (mm)"},{42,30,36,6,14}};
    case A::CoilSpring:return {"Coil spring",{"Wire radius (mm)","Mean coil radius (mm)","Pitch (mm)","Turns (2-12)"},{1.2,8,5,6}};
    case A::TimingPulley:return {"Timing pulley",{"Root radius (mm)","Tip radius (mm)","Face width (mm)","Tooth count (8-60)","Bore radius (mm)"},{18,20,12,24,5}};
    case A::TimingBelt:return {"Timing belt loop",{"Inner radius (mm)","Outer radius (mm)","Width (mm)","Tooth count (12-80)"},{35,39,10,32}};
    case A::BrakeRotor:return {"Brake rotor",{"Rotor radius (mm)","Bore radius (mm)","Disc thickness (mm)","Hat radius (mm)","Hat height (mm)","Bolt circle radius (mm)","Bolt-hole radius (mm)","Bolt count (4-10)"},{50,12,6,24,12,32,3,5}};
    case A::VentedBrakeRotor:return {"Vented brake rotor",{"Rotor radius (mm)","Bore radius (mm)","Plate thickness (mm)","Gap (mm)","Vent post radius (mm)","Vent count (6-24)"},{48,12,4,6,2,12}};
    case A::ClevisBracket:return {"Clevis bracket",{"Inside gap (mm)","Plate thickness (mm)","Arm width (mm)","Arm height (mm)","Pin-hole radius (mm)"},{18,5,20,35,4}};
    case A::HingeBarrel:return {"Hinge barrel",{"Leaf length (mm)","Leaf width (mm)","Leaf thickness (mm)","Pin radius (mm)","Knuckle count (3-7)"},{42,22,3,3,5}};
    case A::PipeUnion:return {"Pipe union",{"Pipe outer radius (mm)","Pipe inner radius (mm)","Body length (mm)","Nut radius (mm)","Nut length (mm)"},{10,7,32,15,9}};
    case A::ValveBody:return {"Valve body",{"Main outer radius (mm)","Main bore radius (mm)","Run length (mm)","Branch radius (mm)","Branch height (mm)"},{14,8,50,12,28}};
    case A::CheckValve:return {"Check valve body",{"Body radius (mm)","Bore radius (mm)","Body length (mm)","Bonnet radius (mm)","Bonnet height (mm)"},{14,7,38,11,18}};
    case A::ReducingNozzle:return {"Reducing nozzle",{"Inlet radius (mm)","Outlet radius (mm)","Length (mm)","Wall thickness (mm)"},{14,6,35,2}};
    case A::HeatSink:return {"Heat sink",{"Base width (mm)","Base depth (mm)","Base thickness (mm)","Fin thickness (mm)","Fin height (mm)","Fin count (3-20)"},{60,40,5,2,22,10}};
    case A::ConveyorRoller:return {"Conveyor roller",{"Outer radius (mm)","Wall thickness (mm)","Length (mm)","Axle radius (mm)","Axle extension (mm)"},{25,2,100,4,10}};
    case A::PumpImpeller:return {"Pump impeller",{"Outer radius (mm)","Hub radius (mm)","Thickness (mm)","Blade count (4-12)","Blade width (mm)"},{30,9,8,6,7}};
    case A::UniversalJoint:return {"Universal joint",{"Yoke radius (mm)","Shaft radius (mm)","Arm length (mm)","Cross radius (mm)","Cross thickness (mm)"},{14,5,24,4,6}};
    case A::Count:break;
    }
    return {};
}

bool SceneDocument::addAdvancedPart(AdvancedPart kind,const QVector<double>& d,QString* error) {
    const auto spec=advancedPartSpec(kind);
    if(spec.name.isEmpty()||d.size()!=spec.defaults.size()){setError(error,"Unknown advanced component or wrong dimension count.");return false;}
    for(double v:d)if(!std::isfinite(v)||v<=1e-5||v>1e6){setError(error,"Advanced component dimensions must be finite and positive.");return false;}
    if(parts_.size()>=2000){setError(error,"Scene object limit reached.");return false;}
    const auto fail=[&](const QString& m){setError(error,m);return false;};
    if((kind==A::ToothLockWasher&&!isCount(d[4],4,16))||(kind==A::FlangeCoupling&&!isCount(d[7],4,12))||
       (kind==A::JawCoupling&&!isCount(d[4],3,8))||(kind==A::CoilSpring&&!isCount(d[3],2,12))||
       ((kind==A::TimingPulley)&&!isCount(d[3],8,60))||(kind==A::TimingBelt&&!isCount(d[3],12,80))||
       (kind==A::BrakeRotor&&!isCount(d[7],4,10))||(kind==A::VentedBrakeRotor&&!isCount(d[5],6,24))||
       (kind==A::HingeBarrel&&!isCount(d[4],3,7))||(kind==A::HeatSink&&!isCount(d[5],3,20))||
       (kind==A::PumpImpeller&&!isCount(d[3],4,12)))return fail("Count values must be whole numbers within the dialog range.");
    if((kind==A::HexBolt||kind==A::SocketHeadScrew||kind==A::ClevisPin)&&d[2]<=d[0])return fail("Head radius must exceed shank radius.");
    if((kind==A::FlatWasher||kind==A::ToothLockWasher)&&d[1]>=d[0])return fail("Washer bore must be smaller than its outer radius.");
    if(kind==A::DowelPin&&(d[2]>=d[1]/2||d[2]>=d[0]))return fail("End chamfer must fit within the pin length and radius.");
    if(kind==A::TaperPin&&d[1]<=d[0])return fail("Large taper radius must exceed small radius.");
    if(kind==A::ShaftCollar&&(d[1]>=d[0]||d[3]>=d[0]-d[1]))return fail("Collar bore and screw must fit in its wall.");
    if((kind==A::FlangeCoupling)&&(d[0]>=d[1]||d[1]>=d[3]||d[6]+d[5]>=d[3]))return fail("Coupling hub and bolt circle dimensions do not fit.");
    if(kind==A::JawCoupling&&(d[0]>=d[1]||d[3]>=d[1]))return fail("Jaw coupling features must fit within the hub.");
    if(kind==A::PillowBlock&&(d[0]>=d[1]/2||d[4]>=d[1]*2))return fail("Bearing bore or base height do not fit the pillow block.");
    if(kind==A::SplitBearingHousing&&(d[0]>=d[2]/2||d[4]*2>=d[3]))return fail("Bearing bore or mounting holes do not fit the housing.");
    if(kind==A::LinearGuideRail&&(d[2]>=d[0]||d[3]>=d[1]*2||d[5]*2>=d[0]))return fail("Guide rail profile or holes do not fit the base.");
    if((kind==A::LinearBearingBlock||kind==A::BallScrewSupport)&&(d[3]>=std::min(d[0],d[1])/2))return fail("Screw bore is too large for the support block.");
    if(kind==A::BallScrewSupport&&(d[4]<=d[3]||d[4]>=d[0]/2))return fail("Bearing boss radius does not fit the block.");
    if(kind==A::CoilSpring&&(d[0]>=d[1]||d[2]<=2*d[0]))return fail("Spring wire and pitch must fit the coil.");
    if((kind==A::TimingPulley||kind==A::TimingBelt)&&(d[0]>=d[1]))return fail("Tooth root radius must be smaller than tip radius.");
    if((kind==A::BrakeRotor||kind==A::VentedBrakeRotor)&&(d[1]>=d[0]*0.7))return fail("Rotor bore must fit within the disc.");
    if(kind==A::ClevisBracket&&(d[1]*2>=d[0]||d[4]>=d[2]/2))return fail("Clevis gap and pin hole do not fit the arms.");
    if(kind==A::HingeBarrel&&d[3]>=d[1]/2)return fail("Hinge pin must fit inside the leaf width.");
    if((kind==A::PipeUnion||kind==A::ValveBody||kind==A::CheckValve||kind==A::ReducingNozzle)&&d[1]>=d[0])return fail("Pipe bore radius must be smaller than the outer radius.");
    if(kind==A::PipeUnion&&d[3]<=d[0])return fail("Union nut must exceed the pipe outer radius.");
    if(kind==A::ReducingNozzle&&(d[0]<=d[3]||d[1]<=d[3]))return fail("Nozzle wall is too thick for the inlet or outlet.");
    if(kind==A::CheckValve&&d[3]>=d[0])return fail("Bonnet radius must be smaller than body radius.");
    if(kind==A::HeatSink&&(d[3]*d[5]>=d[0]-2||d[4]<=d[2]))return fail("Heat sink fins do not fit the base.");
    if(kind==A::ConveyorRoller&&(d[1]>=d[0]||d[3]>=d[0]-d[1]))return fail("Roller wall and axle dimensions do not fit.");
    if(kind==A::PumpImpeller&&(d[1]>=d[0]||d[4]>=d[0]-d[1]))return fail("Impeller hub and blades do not fit.");
    if(kind==A::UniversalJoint&&(d[1]>=d[0]||d[3]>=d[0]||d[4]>=d[0]))return fail("Universal joint cross must fit inside both yokes.");
    try {
        TopoDS_Shape s;
        switch(kind){
        case A::HexBolt:{
            BRepBuilderAPI_MakePolygon h;for(int i=0;i<6;++i){double a=2*pi*i/6+pi/6;h.Add(gp_Pnt(d[4]/std::sqrt(3)*cos(a),d[4]/std::sqrt(3)*sin(a),d[1]));}h.Close();
            s=fuseA(cyl(d[0],d[1]),BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(h.Wire()).Face(),gp_Vec(0,0,d[3])).Shape());break;}
        case A::SocketHeadScrew:{auto body=fuseA(cyl(d[0],d[1]),cyl(d[2],d[3],gp_Pnt(0,0,d[1])));BRepBuilderAPI_MakePolygon h;for(int i=0;i<6;++i){double a=2*pi*i/6;h.Add(gp_Pnt(d[4]*cos(a),d[4]*sin(a),d[1]+d[3]*0.55));}h.Close();auto socket=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(h.Wire()).Face(),gp_Vec(0,0,d[3]*0.5)).Shape();s=cutA(body,socket);break;}
        case A::FlatWasher:s=annulus(d[0],d[1],d[2]);break;
        case A::ToothLockWasher:{auto w=annulus(d[0],d[1],d[2]);for(int i=0;i<static_cast<int>(d[4]);++i){double a=2*pi*i/d[4];auto t=BRepPrimAPI_MakeBox(gp_Pnt(d[0]-d[3]*0.45,-d[2]*0.21,0),d[3],d[2]*0.42,d[2]).Shape();gp_Trsf tr;tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),a);BRepBuilderAPI_Transform placed(t,tr,true);w=fuseA(w,placed.Shape());}s=w;break;}
        case A::DowelPin:{auto main=cyl(d[0],d[1]-d[2]);auto top=BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0,0,d[1]-d[2]),gp_Dir(0,0,1)),d[0],std::max(d[0]-d[2],d[0]*0.25),d[2]).Shape();s=fuseA(main,top);break;}
        case A::ClevisPin:{auto body=fuseA(cyl(d[0],d[1]),cyl(d[2],d[3],gp_Pnt(0,0,d[1])));s=cutA(body,cyl(d[4],2*d[0],gp_Pnt(-d[0],0,d[1]*0.85),gp_Dir(1,0,0)));break;}
        case A::TaperPin:s=BRepPrimAPI_MakeCone(d[0],d[1],d[2]).Shape();break;
        case A::ShaftCollar:{auto c=annulus(d[0],d[1],d[2]);s=cutA(c,cyl(d[3],2*d[0],gp_Pnt(0,0,d[2]/2),gp_Dir(1,0,0)));break;}
        case A::FlangeCoupling:{auto hub=fuseA(cyl(d[1],d[2],gp_Pnt(0,0,-d[2])),cyl(d[3],d[4]));hub=fuseA(hub,cyl(d[1],d[2],gp_Pnt(0,0,d[4])));hub=cutA(hub,cyl(d[0],d[4]+2*d[2],gp_Pnt(0,0,-d[2])));s=boltHoles(hub,d[6],d[5],static_cast<int>(d[7]),0,d[4]);break;}
        case A::JawCoupling:{auto h=fuseA(cyl(d[1],d[2]),cyl(d[1],d[2],gp_Pnt(0,0,d[2])));for(int j=0;j<static_cast<int>(d[4]);++j){double a=2*pi*j/d[4];auto jaw=cyl(d[3],d[2]*0.18,gp_Pnt((d[1]-d[3])*cos(a),(d[1]-d[3])*sin(a),d[2]*0.91));h=fuseA(h,jaw);}s=cutA(h,cyl(d[0],2*d[2],gp_Pnt(0,0,0)));break;}
        case A::PillowBlock:{auto base=BRepPrimAPI_MakeBox(d[3],d[3]*0.68,d[4]).Shape();auto bearing=cyl(d[1],d[2],gp_Pnt(d[3]/2,d[3]*0.34,d[4]),gp_Dir(1,0,0));auto b=fuseA(base,bearing);s=cutA(b,cyl(d[0],d[2]+2,gp_Pnt(d[3]/2-1,d[3]*0.34,d[4]+d[1]),gp_Dir(1,0,0)));break;}
        case A::SplitBearingHousing:{auto b=BRepPrimAPI_MakeBox(d[1],d[3],d[2]).Shape();b=cutA(b,cyl(d[0],d[1]+2,gp_Pnt(-1,d[3]/2,d[2]/2),gp_Dir(1,0,0)));for(double x:{d[1]*0.2,d[1]*0.8})b=cutA(b,cyl(d[4],d[2],gp_Pnt(x,d[3]/2,0)));s=b;break;}
        case A::LinearGuideRail:{auto base=BRepPrimAPI_MakeBox(d[0],d[4],d[1]).Shape();QVector<gp_Pnt> p{{(d[0]-d[2])/2,d[0]*0,d[1]},{(d[0]+d[2])/2,0,d[1]},{(d[0]+d[2]*0.8)/2,0,d[1]+d[3]},{(d[0]-d[2]*0.8)/2,0,d[1]+d[3]}};auto ridge=polyPrism(p,gp_Vec(0,d[4],0));auto rail=fuseA(base,ridge);for(int y=10;y<d[4];y+=20)rail=cutA(rail,cyl(d[5],d[1],gp_Pnt(d[0]/2,y,0)));s=rail;break;}
        case A::LinearBearingBlock:{auto b=BRepPrimAPI_MakeBox(d[0],d[2],d[1]).Shape();b=cutA(b,cyl(d[3],d[2]+2,gp_Pnt(d[0]/2,-1,d[1]/2),gp_Dir(0,1,0)));for(double x:{d[0]*0.2,d[0]*0.8})b=cutA(b,cyl(d[4],d[1],gp_Pnt(x,d[2]/2,0)));s=b;break;}
        case A::BallScrewSupport:{auto b=BRepPrimAPI_MakeBox(d[0],d[2],d[1]).Shape();auto boss=cyl(d[4],d[2]*0.5,gp_Pnt(d[0]/2,d[2]*0.25,d[1]/2),gp_Dir(0,1,0));b=fuseA(b,boss);s=cutA(b,cyl(d[3],d[2]+2,gp_Pnt(d[0]/2,-1,d[1]/2),gp_Dir(0,1,0)));break;}
        case A::CoilSpring:{
            const double turns=d[3],pitch=d[2],major=d[1];const double slope=pitch/(2*pi);const int samples=std::max(48,static_cast<int>(turns*48));
            TColgp_Array1OfPnt points(1,samples+1);
            for(int i=0;i<=samples;++i){const double t=turns*2*pi*i/samples;points.SetValue(i+1,gp_Pnt(major*cos(t),major*sin(t),pitch*t/(2*pi)));}
            GeomAPI_PointsToBSpline interpolation(points);Handle(Geom_BSplineCurve) curve=interpolation.Curve();
            TopoDS_Edge pathEdge=BRepBuilderAPI_MakeEdge(curve).Edge();TopoDS_Wire path=BRepBuilderAPI_MakeWire(pathEdge).Wire();
            const gp_Pnt start(major,0,0);const gp_Dir tangent(0,1,slope);const gp_Circ section(gp_Ax2(start,tangent),d[0]);
            TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(section).Edge()).Wire();
            BRepOffsetAPI_MakePipe pipe(path,BRepBuilderAPI_MakeFace(profile).Face());pipe.Build();if(!pipe.IsDone())throw std::runtime_error("Spring sweep failed.");s=pipe.Shape();break;}
        case A::TimingPulley:s=radialToothedBelt(d[0],d[1],static_cast<int>(d[3]),d[2],d[4]);break;
        case A::TimingBelt:s=radialToothedBelt(d[0],d[1],static_cast<int>(d[3]),d[2],0);break;
        case A::BrakeRotor:{auto disc=annulus(d[0],d[1],d[2]);auto hat=annulus(d[3],d[1],d[4],d[2]);auto rotor=fuseA(disc,hat);s=boltHoles(rotor,d[5],d[6],static_cast<int>(d[7]),0,d[2]+d[4]);break;}
        case A::VentedBrakeRotor:{auto plate1=annulus(d[0],d[1],d[2]);auto plate2=annulus(d[0],d[1],d[2],d[2]+d[3]);auto body=fuseA(plate1,plate2);for(int i=0;i<static_cast<int>(d[5]);++i){double a=2*pi*i/d[5];body=fuseA(body,cyl(d[4],d[3],gp_Pnt((d[0]+d[1])*0.5*cos(a),(d[0]+d[1])*0.5*sin(a),d[2])));}s=body;break;}
        case A::ClevisBracket:{auto base=BRepPrimAPI_MakeBox(gp_Pnt(0,0,0),d[0]+2*d[1],d[2],d[1]).Shape();auto left=BRepPrimAPI_MakeBox(gp_Pnt(0,0,d[1]),d[1],d[2],d[3]).Shape();auto right=BRepPrimAPI_MakeBox(gp_Pnt(d[0]+d[1],0,d[1]),d[1],d[2],d[3]).Shape();auto body=fuseA(fuseA(base,left),right);body=cutA(body,cyl(d[4],d[0]+2*d[1],gp_Pnt(-1,d[2]/2,d[1]+d[3]*0.72),gp_Dir(1,0,0)));s=body;break;}
        case A::HingeBarrel:{
            BRep_Builder builder;TopoDS_Compound assembly;builder.MakeCompound(assembly);const int n=static_cast<int>(d[4]);const double segment=d[0]/n;
            for(int i=0;i<n;++i){const double z=i*segment;const double leafX=(i%2==0)?-d[1]:d[3]*2;builder.Add(assembly,BRepPrimAPI_MakeBox(gp_Pnt(leafX,-d[2]/2,z),d[1],d[2],segment*0.78).Shape());builder.Add(assembly,annulus(d[3]*1.7,d[3],segment*0.9,z));}
            s=assembly;break;}
        case A::PipeUnion:{auto run=annulus(d[0],d[1],d[2]);auto nut=cyl(d[3],d[4],gp_Pnt(0,0,d[2]/2-d[4]/2));auto u=fuseA(run,nut);s=cutA(u,cyl(d[1],d[2]+2*d[4],gp_Pnt(0,0,-d[4])));break;}
        case A::ValveBody:{auto run=cyl(d[0],d[2],gp_Pnt(0,0,-d[2]/2));auto branch=cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0));auto outer=fuseA(run,branch);auto bores=fuseA(cyl(d[1],d[2]+2,gp_Pnt(0,0,-d[2]/2-1)),cyl(d[1],d[4]+2,gp_Pnt(0,0,0),gp_Dir(1,0,0)));s=cutA(outer,bores);break;}
        case A::CheckValve:{auto shell=cyl(d[0],d[2]);auto bonnet=cyl(d[3],d[4],gp_Pnt(0,0,d[2]));auto body=fuseA(shell,bonnet);s=cutA(body,cyl(d[1],d[2]+d[4]+1));break;}
        case A::ReducingNozzle:{auto outer=BRepPrimAPI_MakeCone(d[0],d[1],d[2]).Shape();auto inner=BRepPrimAPI_MakeCone(d[0]-d[3],d[1]-d[3],d[2]).Shape();s=cutA(outer,inner);break;}
        case A::HeatSink:{auto base=BRepPrimAPI_MakeBox(d[0],d[1],d[2]).Shape();const int n=static_cast<int>(d[5]);for(int i=0;i<n;++i){const double x=(i+0.5)*d[0]/n-d[3]/2;base=fuseA(base,BRepPrimAPI_MakeBox(gp_Pnt(x,0,d[2]),d[3],d[1],d[4]).Shape());}s=base;break;}
        case A::ConveyorRoller:{auto drum=annulus(d[0],d[0]-d[1],d[2]);auto axle=cyl(d[3],d[2]+2*d[4],gp_Pnt(0,0,-d[4]));s=fuseA(drum,axle);break;}
        case A::PumpImpeller:{auto plate=cyl(d[0],d[2]);auto hub=cyl(d[1],d[2]*1.5,gp_Pnt(0,0,-d[2]*0.25));auto imp=fuseA(plate,hub);const int n=static_cast<int>(d[3]);for(int i=0;i<n;++i){double a=2*pi*i/n;auto blade=BRepPrimAPI_MakeBox(gp_Pnt(-d[4]/2,d[1],d[2]*0.18),d[4],d[0]-d[1],d[2]*0.65).Shape();gp_Trsf tr;tr.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),a+0.25);BRepBuilderAPI_Transform turned(blade,tr,true);imp=fuseA(imp,turned.Shape());}s=imp;break;}
        case A::UniversalJoint:{
            BRep_Builder builder;TopoDS_Compound joint;builder.MakeCompound(joint);
            auto cross=fuseA(cyl(d[3],d[4],gp_Pnt(0,0,-d[4]/2)),cyl(d[3],d[4],gp_Pnt(0,0,0),gp_Dir(1,0,0)));
            builder.Add(joint,cross);
            for(int sign:{-1,1}){builder.Add(joint,cyl(d[1],d[2],gp_Pnt(0,0,sign*d[0])));builder.Add(joint,BRepPrimAPI_MakeBox(gp_Pnt(-d[1],-d[1]/2,sign*d[0]-d[0]*0.15),d[1]*2,d[1],d[0]*0.35).Shape());}
            s=joint;break;}
        case A::Count:return fail("Unknown advanced component.");
        }
        if(s.IsNull()||!BRepCheck_Analyzer(s).IsValid())return fail("Generated advanced component failed topology validation.");
        GProp_GProps props;BRepGProp::VolumeProperties(s,props);if(!std::isfinite(props.Mass())||props.Mass()<=1e-10)return fail("Generated component has no positive volume.");
        checkpoint();parts_.append({QUuid::createUuid().toString(QUuid::WithoutBraces),uniqueName(spec.name),s,true,"#6c9dc2"});return true;
    }catch(const Standard_Failure& e){return fail(QStringLiteral("Open CASCADE failed: %1").arg(QString::fromUtf8(e.what())));}
     catch(const std::exception& e){return fail(QString::fromUtf8(e.what()));}
}
} // namespace cad
