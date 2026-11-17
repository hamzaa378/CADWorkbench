#include "SketchProfile.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GProp_GProps.hxx>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <cmath>
#include <algorithm>
#include <limits>

namespace cad {
namespace {
void fail(QString* error,const QString& message){if(error)*error=message;}
bool finiteCoordinate(double n){return std::isfinite(n)&&std::abs(n)<=1e6;}
double cross(const QPointF& a,const QPointF& b,const QPointF& c){
    return (b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x());
}
double pointSegmentDistance(const QPointF& p,const QPointF& a,const QPointF& b){
    const double x=b.x()-a.x(),y=b.y()-a.y(),length=x*x+y*y;
    if(length<1e-16)return std::hypot(p.x()-a.x(),p.y()-a.y());
    const double t=std::clamp(((p.x()-a.x())*x+(p.y()-a.y())*y)/length,0.0,1.0);
    return std::hypot(p.x()-a.x()-t*x,p.y()-a.y()-t*y);
}
bool intersects(const QPointF& a,const QPointF& b,const QPointF& c,const QPointF& d){
    constexpr double eps=1e-8;
    const double x=cross(a,b,c),y=cross(a,b,d),z=cross(c,d,a),w=cross(c,d,b);
    if(((x>eps&&y<-eps)||(x<-eps&&y>eps))&&((z>eps&&w<-eps)||(z<-eps&&w>eps)))return true;
    if(std::abs(x)<=eps&&pointSegmentDistance(c,a,b)<=eps)return true;
    if(std::abs(y)<=eps&&pointSegmentDistance(d,a,b)<=eps)return true;
    if(std::abs(z)<=eps&&pointSegmentDistance(a,c,d)<=eps)return true;
    if(std::abs(w)<=eps&&pointSegmentDistance(b,c,d)<=eps)return true;
    return false;
}
}

bool validSketchPlane(const SketchPlane& p,QString* error){
    const double length=std::hypot(p.normalX,std::hypot(p.normalY,p.normalZ));
    if(!finiteCoordinate(p.originX)||!finiteCoordinate(p.originY)||!finiteCoordinate(p.originZ)||
       !std::isfinite(p.normalX)||!std::isfinite(p.normalY)||!std::isfinite(p.normalZ)||
       !std::isfinite(length)||length<1e-8||length>1e6||
       !std::isfinite(p.twistDegrees)||std::abs(p.twistDegrees)>3600){
        fail(error,"Sketch plane needs finite coordinates, a nonzero normal and a valid twist angle.");return false;
    }
    return true;
}
QJsonObject sketchPlaneToJson(const SketchPlane& p){
    return {{"origin_x",p.originX},{"origin_y",p.originY},{"origin_z",p.originZ},
            {"normal_x",p.normalX},{"normal_y",p.normalY},{"normal_z",p.normalZ},
            {"twist_degrees",p.twistDegrees}};
}
bool sketchPlaneFromJson(const QJsonObject& object,SketchPlane* result,QString* error){
    if(!result||object.size()!=7){fail(error,"Invalid sketch datum plane record.");return false;}
    for(const QString& key:{"origin_x","origin_y","origin_z","normal_x","normal_y","normal_z","twist_degrees"}){
        if(!object.value(key).isDouble()){fail(error,"Sketch plane is missing a numeric axis or origin.");return false;}
    }
    SketchPlane p;
    p.originX=object.value("origin_x").toDouble();p.originY=object.value("origin_y").toDouble();
    p.originZ=object.value("origin_z").toDouble();p.normalX=object.value("normal_x").toDouble();
    p.normalY=object.value("normal_y").toDouble();p.normalZ=object.value("normal_z").toDouble();
    p.twistDegrees=object.value("twist_degrees").toDouble();
    if(!validSketchPlane(p,error))return false;
    *result=p;return true;
}

bool buildSketchExtrusion(const SketchModel& sketch,const QStringList& boundary,double depth,
                          const SketchPlane& plane,TopoDS_Shape* output,QString* error){
    if(!output){fail(error,"Missing output for sketch extrusion.");return false;}
    *output=TopoDS_Shape();
    if(!std::isfinite(depth)||depth<=1e-5||depth>1e6||!validSketchPlane(plane,error)||
       boundary.size()<3||boundary.size()>64||sketch.points.size()>64||
       sketch.arcs.size()>32||sketch.splines.size()>32||sketch.circles.size()>64||
       sketch.segments.size()>128||sketch.constraints.size()>128){
        if(error&&error->isEmpty())*error="Sketch geometry exceeds supported dimensions or entity limits.";
        return false;
    }
    const auto solved=SketchSolver::solve(sketch);
    if(!solved.success){fail(error,solved.message);return false;}
    QMap<QString,QPointF> points;
    for(int i=0;i<sketch.points.size();++i){
        if(!finiteCoordinate(solved.solvedPoints[i].x())||!finiteCoordinate(solved.solvedPoints[i].y())){
            fail(error,"Sketch point is outside supported coordinates.");return false;
        }
        points.insert(sketch.points[i].id,solved.solvedPoints[i]);
    }
    QSet<QString> boundarySet;
    for(const auto& id:boundary){
        if(boundarySet.contains(id)||!points.contains(id)){
            fail(error,"Sketch boundary references duplicate or missing vertices.");return false;
        }
        boundarySet.insert(id);
    }
    const auto curveOnBoundary=[&](const QString& from,const QString& to){
        for(int i=0;i<boundary.size();++i)
            if(boundary[i]==from&&boundary[(i+1)%boundary.size()]==to)return true;
        return false;
    };
    QSet<QString> curveEdges;
    for(const auto& a:sketch.arcs){
        if(a.id.isEmpty()||a.id.size()>64||!curveOnBoundary(a.startPointId,a.endPointId)||
           !points.contains(a.midpointId)||a.midpointId==a.startPointId||a.midpointId==a.endPointId||
           curveEdges.contains(a.startPointId+"\n"+a.endPointId)){
            fail(error,"Arc must have a distinct mid-point and match one directed boundary edge.");return false;
        }
        curveEdges.insert(a.startPointId+"\n"+a.endPointId);
    }
    for(const auto& s:sketch.splines){
        if(s.id.isEmpty()||s.id.size()>64||!curveOnBoundary(s.startPointId,s.endPointId)||
           !points.contains(s.control1PointId)||!points.contains(s.control2PointId)||
           curveEdges.contains(s.startPointId+"\n"+s.endPointId)){
            fail(error,"Bezier spline must have two existing handles and one unused boundary edge.");return false;
        }
        curveEdges.insert(s.startPointId+"\n"+s.endPointId);
    }
    try{
        BRepBuilderAPI_MakeWire wire;
        QVector<QPointF> perimeter;
        auto p3=[&](const QString& id){const auto p=points.value(id);return gp_Pnt(p.x(),p.y(),0);};
        for(int i=0;i<boundary.size();++i){
            const QString start=boundary[i],end=boundary[(i+1)%boundary.size()];
            if(std::hypot(points[start].x()-points[end].x(),points[start].y()-points[end].y())<1e-5){
                fail(error,"Sketch outline contains a zero-length edge.");return false;
            }
            perimeter.append(points[start]);
            TopoDS_Edge edge;
            bool curved=false;
            for(const auto& a:sketch.arcs)if(a.startPointId==start&&a.endPointId==end){
                const GC_MakeArcOfCircle arc(p3(start),p3(a.midpointId),p3(end));
                if(!arc.IsDone()){fail(error,"Circular arc cannot be constructed from its three points.");return false;}
                const auto curve=arc.Value();
                const double first=curve->FirstParameter(),last=curve->LastParameter();
                for(int j=1;j<24;++j){const auto p=curve->Value(first+(last-first)*j/24.0);perimeter.append(QPointF(p.X(),p.Y()));}
                edge=BRepBuilderAPI_MakeEdge(curve).Edge();curved=true;break;
            }
            if(!curved)for(const auto& spline:sketch.splines)if(spline.startPointId==start&&spline.endPointId==end){
                TColgp_Array1OfPnt controls(1,4);controls.SetValue(1,p3(start));
                controls.SetValue(2,p3(spline.control1PointId));controls.SetValue(3,p3(spline.control2PointId));
                controls.SetValue(4,p3(end));
                Handle(Geom_BezierCurve) curve=new Geom_BezierCurve(controls);
                for(int j=1;j<24;++j){const auto p=curve->Value(j/24.0);perimeter.append(QPointF(p.X(),p.Y()));}
                edge=BRepBuilderAPI_MakeEdge(curve).Edge();curved=true;break;
            }
            if(!curved)edge=BRepBuilderAPI_MakeEdge(p3(start),p3(end)).Edge();
            wire.Add(edge);
            if(!wire.IsDone()){fail(error,"Sketch edge connection could not form a closed wire.");return false;}
        }
        if(!wire.IsDone()){fail(error,"Sketch wire is invalid.");return false;}
        const int n=perimeter.size();
        double area=0;
        for(int i=0;i<n;++i){
            const QPointF& a=perimeter[i],&b=perimeter[(i+1)%n];
            area+=a.x()*b.y()-a.y()*b.x();
        }
        if(std::abs(area)<1e-8){fail(error,"Sketch boundary has zero enclosed area.");return false;}
        // Conservative planar self-crossing test, with all curve spans sampled.
        for(int i=0;i<n;++i)for(int j=i+2;j<n;++j){
            if(i==0&&j==n-1)continue;
            if(intersects(perimeter[i],perimeter[(i+1)%n],perimeter[j],perimeter[(j+1)%n])){
                fail(error,"Sketch boundary intersects itself.");return false;
            }
        }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        struct Hole {QPointF center;double radius;};
        QVector<Hole> holes;
        for(int k=0;k<sketch.circles.size();++k){
            const auto& circle=sketch.circles[k];if(!circle.cutout)continue;
            if(!points.contains(circle.centerPointId)||k>=solved.solvedCircleRadii.size()){
                fail(error,"Circular hole references a missing sketch center.");return false;
            }
            const QPointF center=points[circle.centerPointId];
            const double r=solved.solvedCircleRadii[k];
            if(!std::isfinite(r)||r<1e-5||r>1e6){fail(error,"Sketch circle radius is invalid.");return false;}
            bool inside=false;double distance=std::numeric_limits<double>::infinity();
            for(int i=0,j=n-1;i<n;j=i++){
                const auto& a=perimeter[i];const auto& b=perimeter[j];
                if((a.y()>center.y())!=(b.y()>center.y())&&
                   center.x()<(b.x()-a.x())*(center.y()-a.y())/(b.y()-a.y())+a.x())inside=!inside;
                distance=std::min(distance,pointSegmentDistance(center,a,b));
            }
            if(!inside||distance<=r+1e-4){fail(error,"Hole must be inside the curved outline, clear of the boundary.");return false;}
            for(const auto& other:holes)if(std::hypot(center.x()-other.center.x(),center.y()-other.center.y())<=r+other.radius+1e-4){
                fail(error,"Circular sketch holes overlap.");return false;
            }
            holes.append({center,r});
            const gp_Circ c(gp_Ax2(gp_Pnt(center.x(),center.y(),0),gp_Dir(0,0,1)),r);
            TopoDS_Wire ring=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(c).Edge()).Wire();
            if(area>0)ring.Reverse();
            face.Add(ring);
        }
        if(!face.IsDone()||!BRepCheck_Analyzer(face.Face()).IsValid()){
            fail(error,"Sketch profile is not a valid planar face.");return false;
        }
        BRepPrimAPI_MakePrism prism(face.Face(),gp_Vec(0,0,depth));
        if(!prism.IsDone()){fail(error,"Extrusion could not be built.");return false;}
        // Deterministic local frame chosen from world X, falling back to Y.
        gp_Vec normal(plane.normalX,plane.normalY,plane.normalZ);normal.Normalize();
        gp_Vec axis=std::abs(normal.X())<0.9?gp_Vec(1,0,0):gp_Vec(0,1,0);
        axis-=normal.Multiplied(axis.Dot(normal));axis.Normalize();
        gp_Vec side=normal.Crossed(axis);
        const double angle=plane.twistDegrees*std::acos(-1.0)/180.0;
        const gp_Vec rotated=axis.Multiplied(std::cos(angle))+side.Multiplied(std::sin(angle));
        const gp_Vec y=normal.Crossed(rotated);
        gp_Trsf toWorld;
        toWorld.SetValues(rotated.X(),y.X(),normal.X(),plane.originX,
                          rotated.Y(),y.Y(),normal.Y(),plane.originY,
                          rotated.Z(),y.Z(),normal.Z(),plane.originZ);
        const TopoDS_Shape shape=BRepBuilderAPI_Transform(prism.Shape(),toWorld,true).Shape();
        if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid()){
            fail(error,"Extrusion on the sketch plane created invalid topology.");return false;
        }
        GProp_GProps properties;BRepGProp::VolumeProperties(shape,properties);
        if(!std::isfinite(properties.Mass())||properties.Mass()<=1e-9){fail(error,"Sketch extrusion has zero volume.");return false;}
        *output=shape;return true;
    }catch(const Standard_Failure& exception){fail(error,QStringLiteral("OCCT sketch builder error: %1").arg(exception.what()));}
     catch(const std::exception& exception){fail(error,QStringLiteral("Sketch extrusion error: %1").arg(exception.what()));}
    return false;
}
}
