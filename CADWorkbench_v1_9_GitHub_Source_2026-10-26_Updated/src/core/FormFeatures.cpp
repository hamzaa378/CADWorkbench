#include "SceneDocument.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr int kMaxObjects=2000;
QString generatedId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
bool positive(double value) { return std::isfinite(value) && value > 0.0 && value <= 1.0e6; }
bool offset(double value) { return std::isfinite(value) && std::abs(value) <= 1.0e6; }

TopoDS_Wire circleWire(double radius,double x,double y,double z) {
    const gp_Ax2 frame(gp_Pnt(x,y,z),gp_Dir(0,0,1));
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,radius)).Edge()).Wire();
}
TopoDS_Wire ellipseWire(double a,double b,double x,double y,double z) {
    const gp_Ax2 frame(gp_Pnt(x,y,z),gp_Dir(0,0,1));
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(frame,std::max(a,b),std::min(a,b))).Edge()).Wire();
}
TopoDS_Wire rectangleWire(double width,double depth,double x,double y,double z) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(x-width/2,y-depth/2,z));
    polygon.Add(gp_Pnt(x+width/2,y-depth/2,z));
    polygon.Add(gp_Pnt(x+width/2,y+depth/2,z));
    polygon.Add(gp_Pnt(x-width/2,y+depth/2,z));
    polygon.Close();
    if(!polygon.IsDone()) throw std::runtime_error("Could not construct the rectangular loft section.");
    return polygon.Wire();
}
TopoDS_Shape loft(const TopoDS_Wire& lower,const TopoDS_Wire& upper) {
    BRepOffsetAPI_ThruSections builder(true,false);
    builder.AddWire(lower);builder.AddWire(upper);builder.Build();
    if(!builder.IsDone()) throw std::runtime_error("Open CASCADE could not build the loft between these sections.");
    return builder.Shape();
}
}

namespace cad {
bool SceneDocument::addEllipticalPrism(double semiAxisA,double semiAxisB,double height,QString* error) {
    if(!positive(semiAxisA)||!positive(semiAxisB)||!positive(height)){
        setError(error,"Ellipse semi-axes and extrusion height must be positive and no greater than 1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Face face=BRepBuilderAPI_MakeFace(ellipseWire(semiAxisA,semiAxisB,0,0,0)).Face();
        const TopoDS_Shape shape=BRepPrimAPI_MakePrism(face,gp_Vec(0,0,height)).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Elliptical Prism"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Ellipse extrusion kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addEllipticalTube(double outerA,double outerB,double innerA,double innerB,double height,QString* error) {
    if(!positive(outerA)||!positive(outerB)||!positive(innerA)||!positive(innerB)||!positive(height)||
       innerA>=outerA||innerB>=outerB){
        setError(error,"Tube dimensions must be positive; each inner ellipse semi-axis must be smaller than its matching outer semi-axis.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Face outerFace=BRepBuilderAPI_MakeFace(ellipseWire(outerA,outerB,0,0,0)).Face();
        const TopoDS_Face innerFace=BRepBuilderAPI_MakeFace(ellipseWire(innerA,innerB,0,0,0)).Face();
        const TopoDS_Shape outer=BRepPrimAPI_MakePrism(outerFace,gp_Vec(0,0,height)).Shape();
        const TopoDS_Shape inner=BRepPrimAPI_MakePrism(innerFace,gp_Vec(0,0,height)).Shape();
        BRepAlgoAPI_Cut cut(outer,inner);cut.Build();
        if(!cut.IsDone()){setError(error,"Open CASCADE could not cut the inner elliptical passage.");return false;}
        const TopoDS_Shape shape=cut.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Elliptical Tube"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Elliptical tube kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftCircularSections(double bottomRadius,double topRadius,double height,double offsetX,double offsetY,QString* error) {
    if(!positive(bottomRadius)||!positive(topRadius)||!positive(height)||!offset(offsetX)||!offset(offsetY)){
        setError(error,"Loft radii and height must be positive; section offsets must be finite and within ±1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=loft(circleWire(bottomRadius,0,0,0),circleWire(topRadius,offsetX,offsetY,height));
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Circular Loft"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Circular loft kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftRectangleToCircle(double width,double depth,double radius,double height,double offsetX,double offsetY,QString* error) {
    if(!positive(width)||!positive(depth)||!positive(radius)||!positive(height)||!offset(offsetX)||!offset(offsetY)){
        setError(error,"Section dimensions and height must be positive; offsets must be finite and within ±1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=loft(rectangleWire(width,depth,0,0,0),circleWire(radius,offsetX,offsetY,height));
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Rectangular to Circular Loft"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Rectangle-to-circle loft kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftHollowReducer(double bottomOuter,double bottomInner,double topOuter,double topInner,
                                     double height,double offsetX,double offsetY,QString* error) {
    if(!positive(bottomOuter)||!positive(bottomInner)||!positive(topOuter)||!positive(topInner)||!positive(height)||
       !offset(offsetX)||!offset(offsetY)||bottomInner>=bottomOuter||topInner>=topOuter){
        setError(error,"Reducer dimensions must be positive, inner radii must be smaller than their matching outer radii, and offsets must be finite.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape outer=loft(circleWire(bottomOuter,0,0,0),circleWire(topOuter,offsetX,offsetY,height));
        const TopoDS_Shape inner=loft(circleWire(bottomInner,0,0,0),circleWire(topInner,offsetX,offsetY,height));
        BRepAlgoAPI_Cut cut(outer,inner);cut.Build();
        if(!cut.IsDone()){setError(error,"Open CASCADE could not form the hollow reducer passage.");return false;}
        const TopoDS_Shape shape=cut.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Hollow Loft Reducer"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Hollow reducer kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}
} // namespace cad
