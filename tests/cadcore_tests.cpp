#include "core/SceneDocument.h"
#include "core/DrawingSheet.h"
#include "core/SheetMetal.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <sstream>

namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void closeTo(double actual, double expected, double tolerance = 1e-5) {
    require(std::isfinite(actual) && std::abs(actual-expected) <= tolerance*std::max(1.,std::abs(expected)),
            ("Numeric mismatch: actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected)).c_str());
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape,props);
    return props.Mass();
}
void validate(const cad::Part& part) {
    require(!part.shape.IsNull() && BRepCheck_Analyzer(part.shape).IsValid(),"Topology invalid");
    require(volume(part.shape)>0,"Non-positive volume");
}
QByteArray readBytes(const QString& filename) {
    QFile file(filename); require(file.open(QIODevice::ReadOnly),"Cannot open file");
    return file.readAll();
}
void writeBytes(const QString& filename,const QByteArray& bytes) {
    QFile file(filename); require(file.open(QIODevice::WriteOnly),"Cannot create file");
    require(file.write(bytes)==bytes.size(),"Write mismatch");
}
void testWingRib() {
    QString error;
    const auto spec=cad::productionPartSpec(cad::ProductionPart::Wingrib);
    require(spec.name=="Wing rib"&&spec.defaults.size()==5,"Wing rib specification is incomplete");
    require(spec.labels==QStringList({"Chord length (mm)","Maximum airfoil depth (mm)","Rib thickness (mm)","Spar opening radius (mm)","Lightening hole radius (mm)"}),
            "Wing rib labels do not describe airfoil parameters");
    cad::SceneDocument doc;
    require(doc.addProductionPart(cad::ProductionPart::Wingrib,spec.defaults,&error),
            ("Default Wing rib failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Wing rib","Wing rib was not inserted");
    const auto& part=doc.parts().first();
    validate(part);
    int solids=0,faces=0;
    for(TopExp_Explorer it(part.shape,TopAbs_SOLID);it.More();it.Next())++solids;
    for(TopExp_Explorer it(part.shape,TopAbs_FACE);it.More();it.Next())++faces;
    require(solids==1,"Wing rib should be one continuous solid");
    require(faces>20,"Wing rib is missing the airfoil profile or through-hole detail");
    Bnd_Box bounds;
    BRepBndLib::Add(part.shape,bounds);
    double xmin,ymin,zmin,xmax,ymax,zmax;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmax-xmin,spec.defaults[0],1e-4);
    closeTo(ymax-ymin,spec.defaults[1],0.01);
    closeTo(zmax-zmin,spec.defaults[2],1e-4);
    require(doc.undo()&&doc.parts().isEmpty(),"Wing rib undo failed");

    auto invalid=spec.defaults;
    invalid[4]=20.0;
    require(!doc.addProductionPart(cad::ProductionPart::Wingrib,invalid,&error),"Oversized Wing rib lightening hole was accepted");
    require(doc.parts().isEmpty()&&!doc.canUndo(),"Rejected Wing rib dimensions mutated the document");
}
void testModelingTools() {
    QString error;
    for(int index=0;index<static_cast<int>(cad::ModelingTool::Count);++index){
        const auto kind=static_cast<cad::ModelingTool>(index);
        const auto spec=cad::modelingToolSpec(kind);
        require(!spec.name.isEmpty()&&!spec.defaults.isEmpty()&&spec.defaults.size()==spec.labels.size(),
                "Modeling-tool specification is incomplete");
        cad::SceneDocument doc;
        auto invalid=spec.defaults; invalid[0]=0;
        require(!doc.addModelingTool(kind,invalid,&error),"Modeling tool accepted a zero dimension");
        require(doc.parts().isEmpty()&&!doc.canUndo(),"Rejected modeling tool changed the document");
        const bool created=doc.addModelingTool(kind,spec.defaults,&error);
        if(!created){std::string dimensions;for(double value:spec.defaults)dimensions+=" "+std::to_string(value);throw std::runtime_error("Modeling tool failed: "+spec.name.toStdString()+"; "+error.toStdString()+"; dimensions:"+dimensions);}
        require(doc.parts().size()==1&&doc.parts().first().name==spec.name,"Modeling tool inserted the wrong body");
        validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Modeling-tool undo failed");
    }
}
void testSweeps() {
    QString error;
    cad::SceneDocument doc;
    require(!doc.sweepCircularProfile(0,60,20,10,&error)&&doc.parts().isEmpty(),"Sweep accepted a zero-radius profile");
    const bool circleCreated=doc.sweepCircularProfile(3,60,20,12,&error);
    require(circleCreated,("Circular sweep failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Circular Sweep","Circular sweep was not inserted");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Circular sweep undo failed");
    const bool rectangleCreated=doc.sweepRectangularProfile(10,6,60,18,10,&error);
    require(rectangleCreated,("Rectangular sweep failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Rectangular Sweep","Rectangular sweep was not inserted");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Rectangular sweep undo failed");
    require(!doc.sweepEllipticalProfile(0,4,60,18,10,&error)&&doc.parts().isEmpty(),
            "Elliptical sweep accepted a zero semi-axis");
    const bool ellipseCreated=doc.sweepEllipticalProfile(8,4,60,18,10,&error);
    require(ellipseCreated,("Elliptical sweep failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Elliptical Sweep","Elliptical sweep was not inserted");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Elliptical sweep undo failed");
}
void testEllipticalLoft() {
    QString error;
    cad::SceneDocument doc;
    require(!doc.loftEllipticalSections(0,10,8,5,30,0,0,&error)&&doc.parts().isEmpty(),
            "Elliptical loft accepted a zero semi-axis");
    require(!doc.loftEllipticalSections(10,6,5,3,0,0,0,&error)&&doc.parts().isEmpty(),
            "Elliptical loft accepted zero height");
    require(!doc.loftEllipticalSections(10,6,5,3,30,std::numeric_limits<double>::infinity(),0,&error)&&doc.parts().isEmpty(),
            "Elliptical loft accepted an infinite offset");
    require(doc.loftEllipticalSections(10,6,5,3,30,4,-2,&error),
            ("Elliptical loft failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Lofted Ellipse",
            "Elliptical loft did not create the expected body");
    const auto part=doc.parts().first();
    validate(part);
    Bnd_Box bounds;BRepBndLib::AddOptimal(part.shape,bounds,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmin,-10);closeTo(xmax,10);closeTo(ymin,-6);closeTo(ymax,6);closeTo(zmin,0);closeTo(zmax,30);
    const double expected=std::numbers::pi*30.0*(60.0+0.5*(10.0*(-3.0)+6.0*(-5.0))+(-5.0*-3.0)/3.0);
    closeTo(volume(part.shape),expected,1e-4);
    require(doc.undo()&&doc.parts().isEmpty(),"Elliptical loft undo failed");
    require(doc.redo()&&doc.parts().size()==1&&doc.parts().first().name=="Lofted Ellipse",
            "Elliptical loft redo failed");
    validate(doc.parts().first());
}

void testNonUniformScale() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,8,6},&error),"Nonuniform-scale fixture failed");
    const QString id=doc.parts().first().id;
    require(!doc.scaleNonUniform(id,0.0,1,1,&error),"Nonuniform scale accepted a zero factor");
    require(!doc.scaleNonUniform(id,-1,1,1,&error),"Nonuniform scale accepted a negative factor");
    require(!doc.scaleNonUniform(id,1,std::numeric_limits<double>::infinity(),1,&error),
            "Nonuniform scale accepted an infinite factor");
    require(!doc.scaleNonUniform(id,10001,1,1,&error),"Nonuniform scale accepted a factor over the limit");
    require(!doc.scaleNonUniform("missing",2,1,1,&error),"Nonuniform scale accepted a missing body");
    require(doc.scaleNonUniform(id,2,0.5,3,&error),
            ("Nonuniform scale failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());
    Bnd_Box bounds;BRepBndLib::AddOptimal(doc.parts().first().shape,bounds,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmin,0);closeTo(xmax,20);closeTo(ymin,0);closeTo(ymax,4);closeTo(zmin,0);closeTo(zmax,18);
    closeTo(volume(doc.parts().first().shape),1440);
    require(doc.undo(),"Nonuniform scale undo failed");
    closeTo(volume(doc.parts().first().shape),480);
    require(doc.redo(),"Nonuniform scale redo failed");
    closeTo(volume(doc.parts().first().shape),1440);
    require(doc.scaleNonUniform(id,1,1,1,&error),"Identity nonuniform scale should be a no-op");
    require(doc.undo(),"Identity scale unexpectedly added a history step");
    closeTo(volume(doc.parts().first().shape),480);
}

void testCustomAxisCircularPattern() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{2,4,2},&error),"Custom-axis pattern fixture failed");
    const QString id=doc.parts().first().id;
    require(doc.translate(id,10,0,0,&error),"Could not position the custom-axis pattern fixture");
    const int originalHistoryCount=doc.parts().size();
    require(!doc.circularPatternAboutAxis(id,0,0,0,0,0,0,4,90,&error),"Custom-axis pattern accepted a zero direction");
    require(!doc.circularPatternAboutAxis(id,0,0,0,0,0,std::numeric_limits<double>::infinity(),4,90,&error),
            "Custom-axis pattern accepted a non-finite direction");
    require(!doc.circularPatternAboutAxis(id,0,0,0,0,0,1,1,90,&error),"Custom-axis pattern accepted one instance");
    require(!doc.circularPatternAboutAxis(id,0,0,0,0,0,1,4,361,&error),"Custom-axis pattern accepted an angle over 360 degrees");
    require(!doc.circularPatternAboutAxis("missing",0,0,0,0,0,1,4,90,&error),"Custom-axis pattern accepted a missing body");
    require(doc.parts().size()==originalHistoryCount,"Rejected custom-axis pattern mutated the scene");
    require(doc.circularPatternAboutAxis(id,0,0,0,0,0,1,4,360,&error),
            ("Custom-axis circular pattern failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==4,"Custom-axis pattern created the wrong number of bodies");
    const double expectedX[4]={10,-4,-12,0};
    const double expectedY[4]={0,10,-4,-12};
    const double expectedWidth[4]={2,4,2,4};
    const double expectedDepth[4]={4,2,4,2};
    for(int index=0;index<4;++index){
        validate(doc.parts()[index]);closeTo(volume(doc.parts()[index].shape),16);
        Bnd_Box bounds;BRepBndLib::AddOptimal(doc.parts()[index].shape,bounds,false,false);
        double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        closeTo(xmin,expectedX[index]);closeTo(ymin,expectedY[index]);
        closeTo(xmax,expectedX[index]+expectedWidth[index]);closeTo(ymax,expectedY[index]+expectedDepth[index]);
        closeTo(zmin,0);closeTo(zmax,2);
    }
    require(doc.undo()&&doc.parts().size()==1,"Custom-axis pattern undo failed");
    require(doc.redo()&&doc.parts().size()==4,"Custom-axis pattern redo failed");
}

void testAsymmetricChamfer() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Asymmetric-chamfer fixture failed");
    const auto source=doc.parts().first();
    const cad::EdgeReference edge{source.id,source.geometryToken,1};
    cad::ChamferCommand invalid;
    require(!doc.prepareAsymmetricChamfer(edge,0,2,&invalid,&error),"Asymmetric chamfer accepted a zero first distance");
    require(!doc.prepareAsymmetricChamfer(edge,1,std::numeric_limits<double>::infinity(),&invalid,&error),
            "Asymmetric chamfer accepted a non-finite distance");
    require(!doc.prepareAsymmetricChamfer(edge,1,1e6+1,&invalid,&error),"Asymmetric chamfer accepted an over-limit distance");
    require(!doc.prepareAsymmetricChamfer({source.id,QStringLiteral("stale"),1},1,2,&invalid,&error),
            "Asymmetric chamfer accepted a stale edge reference");
    require(!doc.prepareAsymmetricChamfer({source.id,source.geometryToken,999},1,2,&invalid,&error),
            "Asymmetric chamfer accepted a missing edge");
    require(!doc.prepareAsymmetricChamfer(edge,1,2,nullptr,&error),"Asymmetric chamfer accepted a null command output");
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-1000)<1e-5,
            "Rejected asymmetric chamfer changed the source body");
    cad::ChamferCommand command;
    require(doc.prepareAsymmetricChamfer(edge,1,2,&command,&error),
            ("Asymmetric chamfer preparation failed: "+error.toStdString()).c_str());
    require(!command.referenceFace.IsNull()&&command.distance==1&&command.secondDistance==2,
            "Asymmetric chamfer did not retain both distances and its reference face");
    const auto preview=cad::SceneDocument::buildChamferPreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Asymmetric chamfer preview failed: "+preview.error.toStdString()).c_str());
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-1000)<1e-5,
            "Asymmetric chamfer preview mutated the source body");
    const double asymmetricVolume=volume(preview.shape);
    require(asymmetricVolume>0&&asymmetricVolume<1000,"Asymmetric chamfer did not remove corner material");
    cad::ChamferCommand symmetricCommand;
    require(doc.prepareChamfer(edge,1,&symmetricCommand,&error),"Symmetric comparison chamfer preparation failed");
    const auto symmetricPreview=cad::SceneDocument::buildChamferPreview(symmetricCommand);
    require(!symmetricPreview.shape.IsNull()&&BRepCheck_Analyzer(symmetricPreview.shape).IsValid(),
            "Symmetric comparison chamfer preview failed");
    require(std::abs(asymmetricVolume-volume(symmetricPreview.shape))>1e-4,
            "Unequal chamfer distances produced the symmetric result");
    require(doc.commitChamfer(command,preview.shape,&error),"Asymmetric chamfer commit failed");
    validate(doc.parts().first());
    closeTo(volume(doc.parts().first().shape),asymmetricVolume);
    require(doc.undo()&&std::abs(volume(doc.parts().first().shape)-1000)<1e-5,"Asymmetric chamfer undo failed");
    require(doc.redo(),"Asymmetric chamfer redo failed");
    closeTo(volume(doc.parts().first().shape),asymmetricVolume);
    require(doc.undo(),"Asymmetric chamfer cleanup undo failed");
    cad::ChamferCommand staleCommand;
    require(doc.prepareAsymmetricChamfer(edge,1,2,&staleCommand,&error),"Staleness fixture preparation failed");
    require(doc.translate(source.id,1,0,0,&error),"Staleness fixture translation failed");
    require(!doc.commitChamfer(staleCommand,preview.shape,&error),"Asymmetric chamfer accepted a stale preview");
}

void testMultiEdgeBlends() {
    QString error;
    cad::SceneDocument filletDoc;
    require(filletDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Multi-edge fillet fixture failed");
    const auto filletPart=filletDoc.parts().first();
    const QVector<cad::EdgeReference> filletEdges={
        {filletPart.id,filletPart.geometryToken,1},{filletPart.id,filletPart.geometryToken,4}};
    cad::FilletCommand filletCommand;
    require(!filletDoc.prepareFillet(QVector<cad::EdgeReference>{},1,&filletCommand,&error),"Empty fillet edge set accepted");
    require(!filletDoc.prepareFillet(QVector<cad::EdgeReference>{filletEdges[0],filletEdges[0]},1,&filletCommand,&error),
            "Duplicate fillet edge accepted");
    require(!filletDoc.prepareFillet(QVector<cad::EdgeReference>{filletEdges[0],{QStringLiteral("other"),filletPart.geometryToken,4}},
                                     1,&filletCommand,&error),"Mixed-body fillet edge set accepted");
    require(!filletDoc.prepareFillet(QVector<cad::EdgeReference>{filletEdges[0],{filletPart.id,QStringLiteral("stale"),4}},
                                     1,&filletCommand,&error),"Stale member of a fillet edge set accepted");
    require(!filletDoc.prepareFillet(QVector<cad::EdgeReference>{filletEdges[0],{filletPart.id,filletPart.geometryToken,999}},
                                     1,&filletCommand,&error),"Missing member of a fillet edge set accepted");
    require(filletDoc.prepareFillet(filletEdges,1,&filletCommand,&error),
            ("Two-edge fillet preparation failed: "+error.toStdString()).c_str());
    require(filletCommand.sourceEdges.size()==2&&filletCommand.edges.size()==2,"Fillet command did not retain both selected edges");
    const auto filletPreview=cad::SceneDocument::buildFilletPreview(filletCommand);
    require(!filletPreview.shape.IsNull()&&BRepCheck_Analyzer(filletPreview.shape).IsValid(),
            ("Two-edge fillet preview failed: "+filletPreview.error.toStdString()).c_str());
    require(volume(filletPreview.shape)<volume(filletPart.shape)&&volume(filletPreview.shape)>0,
            "Two-edge fillet did not remove a valid amount of material");
    require(filletDoc.commitFillet(filletCommand,filletPreview.shape,&error),"Two-edge fillet commit failed");
    validate(filletDoc.parts().first());
    const double filletVolume=volume(filletDoc.parts().first().shape);
    require(filletDoc.undo()&&std::abs(volume(filletDoc.parts().first().shape)-8000)<1e-5,"Multi-edge fillet undo failed");
    require(filletDoc.redo(),"Multi-edge fillet redo failed");closeTo(volume(filletDoc.parts().first().shape),filletVolume);

    cad::SceneDocument chamferDoc;
    require(chamferDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Multi-edge chamfer fixture failed");
    const auto chamferPart=chamferDoc.parts().first();
    const QVector<cad::EdgeReference> chamferEdges={
        {chamferPart.id,chamferPart.geometryToken,1},{chamferPart.id,chamferPart.geometryToken,4}};
    cad::ChamferCommand chamferCommand;
    require(!chamferDoc.prepareChamfer(QVector<cad::EdgeReference>{},1,&chamferCommand,&error),"Empty chamfer edge set accepted");
    require(!chamferDoc.prepareChamfer(QVector<cad::EdgeReference>{chamferEdges[0],chamferEdges[0]},1,&chamferCommand,&error),
            "Duplicate chamfer edge accepted");
    require(!chamferDoc.prepareChamfer(QVector<cad::EdgeReference>{chamferEdges[0],{chamferPart.id,QStringLiteral("stale"),4}},
                                       1,&chamferCommand,&error),"Stale member of a chamfer edge set accepted");
    require(!chamferDoc.prepareChamfer(QVector<cad::EdgeReference>{chamferEdges[0],{chamferPart.id,chamferPart.geometryToken,999}},
                                       1,&chamferCommand,&error),"Missing member of a chamfer edge set accepted");
    require(chamferDoc.prepareChamfer(chamferEdges,1,&chamferCommand,&error),
            ("Two-edge chamfer preparation failed: "+error.toStdString()).c_str());
    require(chamferCommand.sourceEdges.size()==2&&chamferCommand.edges.size()==2,"Chamfer command did not retain both selected edges");
    const auto chamferPreview=cad::SceneDocument::buildChamferPreview(chamferCommand);
    require(!chamferPreview.shape.IsNull()&&BRepCheck_Analyzer(chamferPreview.shape).IsValid(),
            ("Two-edge chamfer preview failed: "+chamferPreview.error.toStdString()).c_str());
    require(volume(chamferPreview.shape)<volume(chamferPart.shape)&&volume(chamferPreview.shape)>0,
            "Two-edge chamfer did not remove a valid amount of material");
    require(chamferDoc.commitChamfer(chamferCommand,chamferPreview.shape,&error),"Two-edge chamfer commit failed");
    validate(chamferDoc.parts().first());
    const double chamferVolume=volume(chamferDoc.parts().first().shape);
    require(chamferDoc.undo()&&std::abs(volume(chamferDoc.parts().first().shape)-8000)<1e-5,"Multi-edge chamfer undo failed");
    require(chamferDoc.redo(),"Multi-edge chamfer redo failed");closeTo(volume(chamferDoc.parts().first().shape),chamferVolume);
}

void testOrientedExtrusion() {
    QString error;
    struct PlaneCase { cad::SketchPlane plane;double xmin,xmax,ymin,ymax,zmin,zmax; };
    const PlaneCase cases[]={
        {cad::SketchPlane::XY,5,45,-4,26,0,20},
        {cad::SketchPlane::XZ,5,45,-20,0,-4,26},
        {cad::SketchPlane::YZ,0,20,5,45,-4,26}
    };
    for(const auto& test:cases){
        cad::SceneDocument doc;cad::ExtrusionCommand command;
        require(doc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,&command,&error),
                "Oriented extrusion preparation failed");
        command.sketchPlane=test.plane;
        const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
        require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                ("Oriented extrusion preview failed: "+preview.error.toStdString()).c_str());
        closeTo(volume(preview.shape),24000,1e-5);
        Bnd_Box bounds;BRepBndLib::AddOptimal(preview.shape,bounds,false,false);
        double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
        closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
        require(doc.parts().isEmpty(),"Oriented extrusion preview mutated the document");
        require(doc.commitExtrusion(command,preview.shape,&error),"Oriented extrusion commit failed");
        require(doc.undo()&&doc.parts().isEmpty(),"Oriented extrusion undo failed");
        require(doc.redo()&&doc.parts().size()==1,"Oriented extrusion redo failed");
    }
      const PlaneCase offsetCases[]={
          {cad::SketchPlane::XY,5,45,-4,26,7,27},
          {cad::SketchPlane::XZ,5,45,-27,-7,-4,26},
          {cad::SketchPlane::YZ,7,27,5,45,-4,26}
      };
      for(const auto& test:offsetCases){
          cad::SceneDocument offsetDoc;cad::ExtrusionCommand command;
          require(offsetDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,&command,&error),
                  "Offset extrusion preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=7;
          const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
          require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                  "Offset extrusion preview failed");
          closeTo(volume(preview.shape),24000,1e-5);
          Bnd_Box bounds;BRepBndLib::AddOptimal(preview.shape,bounds,false,false);
          double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
          closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
          closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
      }
      const PlaneCase reverseCases[]={
          {cad::SketchPlane::XY,5,45,-4,26,-13,7},
          {cad::SketchPlane::XZ,5,45,-7,13,-4,26},
          {cad::SketchPlane::YZ,-13,7,5,45,-4,26}
      };
      for(const auto& test:reverseCases){
          cad::SceneDocument reverseDoc;cad::ExtrusionCommand command;
          require(reverseDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,&command,&error),
                  "Reverse extrusion preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=7;command.reverseDirection=true;
          const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
          require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                  "Reverse extrusion preview failed");
          closeTo(volume(preview.shape),24000,1e-5);
          Bnd_Box bounds;BRepBndLib::AddOptimal(preview.shape,bounds,false,false);
          double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
          closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
          closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
      }
      const PlaneCase symmetricCases[]={
          {cad::SketchPlane::XY,5,45,-4,26,-3,17},
          {cad::SketchPlane::XZ,5,45,-17,3,-4,26},
          {cad::SketchPlane::YZ,-3,17,5,45,-4,26}
      };
      for(const auto& test:symmetricCases){
          cad::SceneDocument symmetricDoc;cad::ExtrusionCommand command;
          require(symmetricDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,&command,&error),
                  "Symmetric extrusion preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=7;command.symmetric=true;
          const auto forward=cad::SceneDocument::buildExtrusionPreview(command);
          require(!forward.shape.IsNull()&&BRepCheck_Analyzer(forward.shape).IsValid(),
                  "Symmetric extrusion preview failed");
          closeTo(volume(forward.shape),24000,1e-5);
          Bnd_Box forwardBounds;BRepBndLib::AddOptimal(forward.shape,forwardBounds,false,false);
          double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
          forwardBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
          closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
          closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
          command.reverseDirection=true;
          const auto reverse=cad::SceneDocument::buildExtrusionPreview(command);
          require(!reverse.shape.IsNull()&&BRepCheck_Analyzer(reverse.shape).IsValid(),
                  "Symmetric reverse-option preview failed");
          closeTo(volume(reverse.shape),volume(forward.shape),1e-5);
          Bnd_Box reverseBounds;BRepBndLib::AddOptimal(reverse.shape,reverseBounds,false,false);
          double rxmin=0,rymin=0,rzmin=0,rxmax=0,rymax=0,rzmax=0;
          reverseBounds.Get(rxmin,rymin,rzmin,rxmax,rymax,rzmax);
          closeTo(rxmin,xmin);closeTo(rxmax,xmax);closeTo(rymin,ymin);
          closeTo(rymax,ymax);closeTo(rzmin,zmin);closeTo(rzmax,zmax);
      }
      const PlaneCase circleCases[]={
          {cad::SketchPlane::XY,-7,13,-14,6,2,7},
          {cad::SketchPlane::XZ,-7,13,-7,-2,-14,6},
          {cad::SketchPlane::YZ,2,7,-7,13,-14,6}
      };
      for(const auto& test:circleCases){
          cad::SceneDocument circleDoc;cad::ExtrusionCommand command;
          require(circleDoc.prepareExtrusion(cad::SketchProfile::Circle,3,-4,20,20,5,&command,&error),
                  "Oriented circle extrusion preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=7;command.reverseDirection=true;
          const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
          require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                  "Oriented circle extrusion preview failed");
          closeTo(volume(preview.shape),std::numbers::pi*100*5,1e-5);
          Bnd_Box bounds;BRepBndLib::AddOptimal(preview.shape,bounds,false,false);
          double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
          bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
          closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
          closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
      }
      struct ThroughAllCase { cad::SketchPlane plane;double u,v,offset,removed;bool reverse; };
      const ThroughAllCase throughAllCases[]={
          {cad::SketchPlane::XY,10,10,0,500,false},
          {cad::SketchPlane::XZ,10,5,-30,750,false},
          {cad::SketchPlane::YZ,10,5,0,1000,false},
          {cad::SketchPlane::XY,10,10,20,500,true},
          {cad::SketchPlane::XZ,10,5,0,750,true},
          {cad::SketchPlane::YZ,10,5,40,1000,true}
      };
      for(const auto& test:throughAllCases){
          cad::SceneDocument cutDoc;
          require(cutDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Through-all target fixture failed");
          const QString targetId=cutDoc.parts().first().id;
          cad::ExtrusionCommand command;
          require(cutDoc.prepareExtrusion(cad::SketchProfile::Rectangle,test.u,test.v,5,5,1,
                  cad::ExtrusionMode::Cut,targetId,&command,&error),"Through-all Cut preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=test.offset;
          command.reverseDirection=test.reverse;command.throughAll=true;
          const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
          require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                  ("Through-all Cut preview failed: "+preview.error.toStdString()).c_str());
          closeTo(volume(preview.shape),24000-test.removed,1e-5);
          require(cutDoc.commitExtrusion(command,preview.shape,&error),"Through-all Cut commit failed");
          require(cutDoc.undo()&&cutDoc.parts().size()==1,"Through-all Cut undo failed");
          closeTo(volume(cutDoc.parts().first().shape),24000,1e-5);
      }
      const PlaneCase reverseTaperCases[]={
          {cad::SketchPlane::XY,0,40,0,30,-13,7},
          {cad::SketchPlane::XZ,0,40,-7,13,0,30},
          {cad::SketchPlane::YZ,-13,7,0,40,0,30}
      };
      for(const auto& test:reverseTaperCases){
          cad::SceneDocument taperDoc;cad::ExtrusionCommand command;
          require(taperDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,40,30,20,&command,&error),
                  "Reverse tapered extrusion preparation failed");
          command.sketchPlane=test.plane;command.planeOffset=7;command.reverseDirection=true;command.taperDegrees=5;
          const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
          require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
                  "Reverse tapered extrusion preview failed");
          require(volume(preview.shape)>0,"Reverse tapered extrusion has no volume");
          Bnd_Box bounds;BRepBndLib::AddOptimal(preview.shape,bounds,false,false);
          double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
          closeTo(xmin,test.xmin);closeTo(xmax,test.xmax);closeTo(ymin,test.ymin);
          closeTo(ymax,test.ymax);closeTo(zmin,test.zmin);closeTo(zmax,test.zmax);
      }
    cad::ExtrusionCommand invalid;
    cad::SceneDocument doc;
    require(doc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,10,8,5,&invalid,&error),"Invalid-plane fixture failed");
    invalid.sketchPlane=static_cast<cad::SketchPlane>(4);
    require(cad::SceneDocument::buildExtrusionPreview(invalid).shape.IsNull(),"Invalid sketch plane accepted");
      require(doc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,10,8,5,&invalid,&error),"Offset invalid-case fixture failed");
      invalid.planeOffset=std::numeric_limits<double>::quiet_NaN();
      require(cad::SceneDocument::buildExtrusionPreview(invalid).shape.IsNull(),"Non-finite plane offset accepted");
      require(doc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,10,8,5,&invalid,&error),"Through-all invalid-case fixture failed");
      invalid.throughAll=true;
      require(cad::SceneDocument::buildExtrusionPreview(invalid).shape.IsNull(),"Through-all New Body was accepted");
      cad::SceneDocument awayDoc;
      require(awayDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Through-all direction fixture failed");
      require(awayDoc.prepareExtrusion(cad::SketchProfile::Rectangle,10,10,5,5,1,cad::ExtrusionMode::Cut,
              awayDoc.parts().first().id,&invalid,&error),"Through-all direction preparation failed");
      invalid.throughAll=true;invalid.reverseDirection=true;
      require(cad::SceneDocument::buildExtrusionPreview(invalid).shape.IsNull(),"Through-all accepted a direction away from the target");
}

void testHelicalSweep() {
    QString error;
    cad::SceneDocument doc;
    require(!doc.addHelicalSweep(1.5,12,2,6,&error)&&doc.parts().isEmpty(),"Helical sweep accepted pitch smaller than wire diameter");
    require(!doc.addHelicalSweep(1.5,1.5,5,6,&error)&&doc.parts().isEmpty(),"Helical sweep accepted overlapping coil radius");
    require(!doc.addHelicalSweep(1.5,12,5,21,&error)&&doc.parts().isEmpty(),"Helical sweep accepted more than 20 turns");
    const bool created=doc.addHelicalSweep(1.5,12,5,6,&error);
    require(created,("Helical sweep failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Helical Sweep","Helical sweep was not inserted");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Helical sweep undo failed");
    require(doc.redo()&&doc.parts().size()==1&&doc.parts().first().name=="Helical Sweep","Helical sweep redo failed");
}
void testVariablePitchHelicalSweep() {
    QString error;
    cad::SceneDocument doc;
    require(!doc.addVariablePitchHelicalSweep(1.5,12,2,7,6,&error)&&doc.parts().isEmpty(),
            "Variable-pitch helix accepted a start pitch smaller than wire diameter");
    require(!doc.addVariablePitchHelicalSweep(1.5,12,4,2,6,&error)&&doc.parts().isEmpty(),
            "Variable-pitch helix accepted an end pitch smaller than wire diameter");
    require(!doc.addVariablePitchHelicalSweep(1.5,1.5,4,7,6,&error)&&doc.parts().isEmpty(),
            "Variable-pitch helix accepted an overlapping coil radius");
    require(!doc.addVariablePitchHelicalSweep(1.5,12,4,7,21,&error)&&doc.parts().isEmpty(),
            "Variable-pitch helix accepted more than 20 turns");
    const bool created=doc.addVariablePitchHelicalSweep(1.5,12,4,7,6,&error);
    require(created,("Variable-pitch helix failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Variable Pitch Helix",
            "Variable-pitch helix was not inserted with expected name");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Variable-pitch helix undo failed");
    require(doc.redo()&&doc.parts().size()==1&&doc.parts().first().name=="Variable Pitch Helix",
            "Variable-pitch helix redo failed");
}
void testCustomPlaneExtrusion() {
    QString error;
    cad::SceneDocument doc;
    cad::ExtrusionCommand command;
    require(doc.prepareExtrusion(cad::SketchProfile::Rectangle,2,3,8,4,6,&command,&error),
            "Custom-plane extrusion preparation failed");
    command.sketchPlane=cad::SketchPlane::Custom;
    command.customOriginX=10;command.customOriginY=20;command.customOriginZ=30;
    command.customNormalX=1;command.customNormalY=2;command.customNormalZ=2;
    command.customUAxisX=1;command.customUAxisY=0;command.customUAxisZ=0;
    command.planeOffset=2;
    const auto preview=cad::SceneDocument::buildExtrusionPreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Custom-plane preview invalid: "+preview.error.toStdString()).c_str());
    closeTo(volume(preview.shape),192);
    GProp_GProps props;BRepGProp::VolumeProperties(preview.shape,props);
    const double invSqrt18=1.0/std::sqrt(18.0),invSqrt2=1.0/std::sqrt(2.0);
    const double expectedX=10+6*4*invSqrt18+5*0+5.0/3.0;
    const double expectedY=20-6*invSqrt18+5*invSqrt2+10.0/3.0;
    const double expectedZ=30-6*invSqrt18-5*invSqrt2+10.0/3.0;
    closeTo(props.CentreOfMass().X(),expectedX);closeTo(props.CentreOfMass().Y(),expectedY);
    closeTo(props.CentreOfMass().Z(),expectedZ);
    require(doc.parts().isEmpty(),"Custom-plane preview mutated the scene");
    require(doc.commitExtrusion(command,preview.shape,&error),"Custom-plane extrusion commit failed");
    require(doc.parts().size()==1,"Custom-plane extrusion did not create one body");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Custom-plane extrusion undo failed");
    require(doc.redo()&&doc.parts().size()==1,"Custom-plane extrusion redo failed");

    command.customNormalX=0;command.customNormalY=0;command.customNormalZ=0;
    require(cad::SceneDocument::buildExtrusionPreview(command).shape.IsNull(),
            "Zero custom plane normal was accepted");
    command.customNormalX=1;command.customNormalY=0;command.customNormalZ=0;
    command.customUAxisX=2;command.customUAxisY=0;command.customUAxisZ=0;
    require(cad::SceneDocument::buildExtrusionPreview(command).shape.IsNull(),
            "U direction parallel to custom normal was accepted");
    command.customUAxisX=0;command.customUAxisY=1;command.customUAxisZ=0;
    command.customOriginZ=std::numeric_limits<double>::infinity();
    require(cad::SceneDocument::buildExtrusionPreview(command).shape.IsNull(),
            "Non-finite custom origin was accepted");

    cad::SceneDocument target;
    require(target.addPrimitive(cad::Primitive::Box,{40,30,20},&error),
            "Custom through-all target fixture failed");
    cad::ExtrusionCommand cut;
    const QString targetId=target.parts().first().id;
    require(target.prepareExtrusion(cad::SketchProfile::Rectangle,-100,-100,200,200,1,
                cad::ExtrusionMode::Cut,targetId,&cut,&error),
            "Custom through-all cut preparation failed");
    cut.sketchPlane=cad::SketchPlane::Custom;cut.customOriginX=20;cut.customOriginY=15;cut.customOriginZ=10;
    cut.customNormalX=1;cut.customNormalY=1;cut.customNormalZ=1;
    cut.customUAxisX=1;cut.customUAxisY=-1;cut.customUAxisZ=0;cut.throughAll=true;
    const auto cutPreview=cad::SceneDocument::buildExtrusionPreview(cut);
    require(!cutPreview.shape.IsNull()&&BRepCheck_Analyzer(cutPreview.shape).IsValid(),
            ("Custom-plane through-all preview failed: "+cutPreview.error.toStdString()).c_str());
    require(volume(cutPreview.shape)<24000,"Custom-plane through-all Cut did not remove material");
    require(target.commitExtrusion(cut,cutPreview.shape,&error),"Custom-plane through-all commit failed");
    require(target.undo()&&std::abs(volume(target.parts().first().shape)-24000)<1e-5,
            "Custom-plane through-all undo failed");
    cut.reverseDirection=true;
    const auto reversePreview=cad::SceneDocument::buildExtrusionPreview(cut);
    require(!reversePreview.shape.IsNull()&&BRepCheck_Analyzer(reversePreview.shape).IsValid(),
            "Reverse custom-plane through-all preview failed");
    require(volume(reversePreview.shape)<24000,"Reverse custom-plane through-all Cut did not remove material");
}

void testCustomAxisRevolve() {
    QString error;cad::SceneDocument doc;cad::RevolveCommand command;
    require(doc.prepareRevolveAboutCustomAxis(5,2,3,10,360,4,-3,8,1,2,2,&command,&error),
            "Custom-axis revolve preparation failed");
    const auto preview=cad::SceneDocument::buildRevolvePreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Custom-axis revolve preview invalid: "+preview.error.toStdString()).c_str());
    const double expectedVolume=390*std::numbers::pi;
    closeTo(volume(preview.shape),expectedVolume);
    GProp_GProps props;BRepGProp::VolumeProperties(preview.shape,props);
    closeTo(props.CentreOfMass().X(),4+7/std::sqrt(9.0));
    closeTo(props.CentreOfMass().Y(),-3+14/std::sqrt(9.0));
    closeTo(props.CentreOfMass().Z(),8+14/std::sqrt(9.0));
    require(doc.parts().isEmpty(),"Custom-axis revolve preview mutated the scene");
    require(doc.commitRevolve(command,preview.shape,&error),"Custom-axis revolve commit failed");
    require(doc.parts().size()==1,"Custom-axis revolve did not create one body");validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Custom-axis revolve undo failed");
    require(doc.redo()&&doc.parts().size()==1,"Custom-axis revolve redo failed");

    cad::RevolveCommand partial;
    require(doc.prepareRevolveAboutCustomAxis(5,0,3,10,90,-2,4,1,1,2,2,&partial,&error),
            "Partial custom-axis revolve preparation failed");
    const auto quarter=cad::SceneDocument::buildRevolvePreview(partial);
    require(!quarter.shape.IsNull()&&BRepCheck_Analyzer(quarter.shape).IsValid(),
            "90-degree custom-axis revolve was invalid");
    closeTo(volume(quarter.shape),expectedVolume/4.0);

    require(!doc.prepareRevolveAboutCustomAxis(5,0,3,10,360,0,0,0,0,0,0,&partial,&error),
            "Zero custom revolution axis was accepted");
    partial.axisDirectionX=std::numeric_limits<double>::quiet_NaN();
    require(cad::SceneDocument::buildRevolvePreview(partial).shape.IsNull(),
            "Non-finite custom revolution axis was accepted");
}

void testTaperedHelicalSweep() {
    QString error;
    cad::SceneDocument doc;
    require(!doc.addTaperedHelicalSweep(1.5,1.5,12,5,6,&error)&&doc.parts().isEmpty(),
            "Tapered helix accepted a start coil radius smaller than wire radius");
    require(!doc.addTaperedHelicalSweep(1.5,12,1.5,5,6,&error)&&doc.parts().isEmpty(),
            "Tapered helix accepted an end coil radius smaller than wire radius");
    require(!doc.addTaperedHelicalSweep(1.5,8,16,2,6,&error)&&doc.parts().isEmpty(),
            "Tapered helix accepted pitch smaller than wire diameter");
    require(!doc.addTaperedHelicalSweep(1.5,8,16,5,21,&error)&&doc.parts().isEmpty(),
            "Tapered helix accepted more than 20 turns");
    const bool created=doc.addTaperedHelicalSweep(1.5,8,16,5,6,&error);
    require(created,("Tapered helix failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==1&&doc.parts().first().name=="Tapered Helix","Tapered helix was not inserted");
    validate(doc.parts().first());
    require(doc.undo()&&doc.parts().isEmpty(),"Tapered helix undo failed");
    require(doc.redo()&&doc.parts().size()==1&&doc.parts().first().name=="Tapered Helix","Tapered helix redo failed");
}
void testFormFeatures() {
    QString error;
    {
        cad::SceneDocument doc;
        require(!doc.addEllipticalPrism(0,10,25,&error)&&doc.parts().isEmpty(),"Elliptical prism accepted a zero semi-axis");
        require(doc.addEllipticalPrism(16,9,25,&error),("Elliptical prism failed: "+error.toStdString()).c_str());
        require(doc.parts().first().name=="Elliptical Prism","Elliptical prism name mismatch");validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Elliptical prism undo failed");
    }
    {
        cad::SceneDocument doc;
        require(!doc.addEllipticalTube(18,12,18,7,30,&error)&&doc.parts().isEmpty(),"Elliptical tube accepted an inner axis equal to its outer axis");
        require(doc.addEllipticalTube(18,12,12,7,30,&error),("Elliptical tube failed: "+error.toStdString()).c_str());
        require(doc.parts().first().name=="Elliptical Tube","Elliptical tube name mismatch");validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Elliptical tube undo failed");
    }
    {
        cad::SceneDocument doc;
        require(!doc.loftCircularSections(16,9,0,0,0,&error)&&doc.parts().isEmpty(),"Circular loft accepted zero height");
        require(doc.loftCircularSections(16,9,35,4,-2,&error),("Circular loft failed: "+error.toStdString()).c_str());
        require(doc.parts().first().name=="Circular Loft","Circular loft name mismatch");validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Circular loft undo failed");
    }
    {
        cad::SceneDocument doc;
        require(!doc.loftRectangleToCircle(35,25,0,30,0,0,&error)&&doc.parts().isEmpty(),"Rectangular-to-circular loft accepted zero radius");
        require(doc.loftRectangleToCircle(35,25,12,30,2,3,&error),("Rectangular-to-circular loft failed: "+error.toStdString()).c_str());
        require(doc.parts().first().name=="Rectangular to Circular Loft","Transition loft name mismatch");validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Transition loft undo failed");
    }
    {
        cad::SceneDocument doc;
        require(!doc.loftHollowReducer(18,18,10,6,35,0,0,&error)&&doc.parts().isEmpty(),"Hollow reducer accepted equal inner and outer radius");
        require(doc.loftHollowReducer(18,12,10,6,35,1,0,&error),("Hollow reducer failed: "+error.toStdString()).c_str());
        require(doc.parts().first().name=="Hollow Loft Reducer","Hollow reducer name mismatch");validate(doc.parts().first());
        require(doc.undo()&&doc.parts().isEmpty(),"Hollow reducer undo failed");
    }
}
void testVectorLinearPattern() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create vector-pattern source body");
    const QString id=doc.parts().first().id;
    require(!doc.linearPatternAlongVector(id,0,0,0,3,10,&error)&&doc.parts().size()==1,
            "Linear pattern accepted a zero direction vector");
    require(!doc.linearPatternAlongVector(id,1,1,0,1,10,&error)&&doc.parts().size()==1,
            "Linear pattern accepted fewer than two instances");
    require(!doc.linearPatternAlongVector(id,1,0,0,3,0,&error)&&doc.parts().size()==1,
            "Linear pattern accepted zero spacing");
    require(!doc.linearPatternAlongVector("missing",1,0,0,3,10,&error)&&doc.parts().size()==1,
            "Linear pattern accepted a missing source body");
    require(doc.linearPatternAlongVector(id,1,1,0,3,10,&error),
            ("Diagonal linear pattern failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==3&&doc.parts()[1].name.contains("Vector Pattern"),
            "Vector pattern did not insert the expected instances");
    for(const auto& part:doc.parts())validate(part);
    Bnd_Box bounds;BRepBndLib::Add(doc.parts()[1].shape,bounds);
    double xmin,ymin,zmin,xmax,ymax,zmax;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmin,10/std::sqrt(2.0));closeTo(ymin,10/std::sqrt(2.0));
    require(doc.undo()&&doc.parts().size()==1,"Vector pattern did not undo as one operation");
    require(doc.redo()&&doc.parts().size()==3,"Vector pattern redo failed");
}
void testVectorRectangularPattern(){
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Could not create vector rectangular-pattern source");
    const QString id=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    require(!doc.rectangularPatternAlongVectors(id,0,0,0,2,10,0,1,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted a zero first direction");
    require(!doc.rectangularPatternAlongVectors(id,1,0,0,2,10,0,0,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted a zero second direction");
    require(!doc.rectangularPatternAlongVectors(id,1,0,0,2,10,2,0,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted parallel directions");
    require(!doc.rectangularPatternAlongVectors(id,std::numeric_limits<double>::infinity(),0,0,2,10,0,1,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted non-finite direction data");
    require(!doc.rectangularPatternAlongVectors(id,1,0,0,2,0,0,1,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted zero spacing for a repeated direction");
    require(!doc.rectangularPatternAlongVectors(id,1,0,0,1,0,2,0,0,1,0,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted a single total instance");
    require(!doc.rectangularPatternAlongVectors(id,1,0,0,34,10,0,1,0,30,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted more than 1,000 total instances");
    require(!doc.rectangularPatternAlongVectors("missing",1,0,0,2,10,0,1,0,2,10,&error)&&doc.parts().size()==1,
            "Vector rectangular pattern accepted a missing source body");
    closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.rectangularPatternAlongVectors(id,1,1,0,2,10,0,0,1,3,20,&error),
            ("Diagonal vector rectangular pattern failed: "+error.toStdString()).c_str());
    require(doc.parts().size()==6,"Vector rectangular pattern instance count mismatch");
    QSet<QString> names;
    for(const auto& part:doc.parts()){
        validate(part);closeTo(volume(part.shape),originalVolume);
        require(!names.contains(part.name.toCaseFolded()),"Vector rectangular pattern produced duplicate names");
        names.insert(part.name.toCaseFolded());
    }
    Bnd_Box zCopy;BRepBndLib::AddOptimal(doc.parts()[1].shape,zCopy,false,false);
    double x0,y0,z0,x1,y1,z1;zCopy.Get(x0,y0,z0,x1,y1,z1);closeTo(z0,20);closeTo(z1,24);
    Bnd_Box diagonalCopy;BRepBndLib::AddOptimal(doc.parts()[3].shape,diagonalCopy,false,false);
    diagonalCopy.Get(x0,y0,z0,x1,y1,z1);closeTo(x0,10/std::sqrt(2.0));closeTo(y0,10/std::sqrt(2.0));
    require(doc.undo()&&doc.parts().size()==1,"Vector rectangular pattern did not undo atomically");
    require(doc.redo()&&doc.parts().size()==6,"Vector rectangular pattern redo failed");
    cad::SceneDocument oneDirection;
    require(oneDirection.addPrimitive(cad::Primitive::Box,{1,1,1},&error),"Single-direction pattern fixture failed");
    const QString oneId=oneDirection.parts().first().id;
    require(oneDirection.rectangularPatternAlongVectors(oneId,1,0,0,1,0,0,0,1,3,5,&error)&&oneDirection.parts().size()==3,
            "Vector rectangular pattern rejected a one-instance axis paired with a repeated axis");
}
void testMinimumDistanceMeasurement() {
    cad::SceneDocument doc; QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create first clearance body");
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create second clearance body");
    const QString first=doc.parts()[0].id,second=doc.parts()[1].id;
    require(doc.translate(second,15,0,0,&error),"Could not position second clearance body");
    const bool undoBefore=doc.canUndo();
    cad::MinimumDistanceResult result;
    require(!doc.measureMinimumDistance(first,first,&result,&error),"Distance measurement accepted the same body twice");
    require(!doc.measureMinimumDistance(first,"missing",&result,&error),"Distance measurement accepted a missing body");
    require(!doc.measureMinimumDistance(first,second,nullptr,&error),"Distance measurement accepted a null result");
    require(doc.measureMinimumDistance(first,second,&result,&error),("Separated-body distance failed: "+error.toStdString()).c_str());
    closeTo(result.distance,5);closeTo(result.firstX,10);closeTo(result.secondX,15);
    closeTo(result.firstY,result.secondY);closeTo(result.firstZ,result.secondZ);
    require(doc.canUndo()==undoBefore&&doc.parts().size()==2,"Read-only distance measurement changed document history or bodies");
    require(doc.translate(second,-7,0,0,&error),"Could not overlap clearance test bodies");
    require(doc.measureMinimumDistance(first,second,&result,&error),"Intersecting-body distance measurement failed");
    closeTo(result.distance,0);
    require(doc.translate(second,0,30,0,&error),"Could not reposition clearance body vertically");
    require(doc.measureMinimumDistance(first,second,&result,&error),"Vertical clearance measurement failed");
    closeTo(result.distance,18);closeTo(result.secondY-result.firstY,18);
    require(doc.translate(second,0,-18,0,&error),"Could not position touching clearance body");
    require(doc.measureMinimumDistance(first,second,&result,&error),"Touching-body clearance measurement failed");
    closeTo(result.distance,0);
}
void testInterferenceMeasurement() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create first interference body");
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create second interference body");
    const QString a=doc.parts()[0].id,b=doc.parts()[1].id;
    require(doc.translate(b,5,0,0,&error),"Could not overlap interference bodies");
    const bool undoBefore=doc.canUndo();cad::InterferenceResult result;
    const double firstVolumeBefore=volume(doc.find(a)->shape),secondVolumeBefore=volume(doc.find(b)->shape);
    require(!doc.measureInterference(a,a,&result,&error),"Interference measurement accepted duplicate body");
    require(!doc.measureInterference(a,"missing",&result,&error),"Interference measurement accepted missing body");
    require(!doc.measureInterference(a,b,nullptr,&error),"Interference measurement accepted null result");
    require(doc.measureInterference(a,b,&result,&error),("Overlap measurement failed: "+error.toStdString()).c_str());
    closeTo(result.volume,840);
    closeTo(result.centroidX,7.5);closeTo(result.centroidY,6);closeTo(result.centroidZ,7);
    closeTo(volume(doc.find(a)->shape),firstVolumeBefore);closeTo(volume(doc.find(b)->shape),secondVolumeBefore);
    require(doc.canUndo()==undoBefore&&doc.parts().size()==2,"Interference measurement changed document history");
    require(doc.translate(b,10,0,0,&error),"Could not separate interference bodies");
    require(doc.measureInterference(a,b,&result,&error),"Disjoint-body interference query failed");closeTo(result.volume,0);
    require(doc.translate(b,-5,0,0,&error),"Could not position touching interference bodies");
    require(doc.measureInterference(a,b,&result,&error),"Touching-body interference query failed");closeTo(result.volume,0);
    require(doc.addPrimitive(cad::Primitive::Box,{8,9,10},&error),"Could not create contained interference body");
    const QString inner=doc.parts().last().id;
    require(doc.translate(inner,1,1,1,&error),"Could not position contained interference body");
    require(doc.measureInterference(a,inner,&result,&error),"Contained-body interference query failed");closeTo(result.volume,720);
    closeTo(result.centroidX,5);closeTo(result.centroidY,5.5);closeTo(result.centroidZ,6);
}

void testBodyDiagnostics() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create diagnostics test box");
    const QString id=doc.parts().first().id;
    const bool undoBefore=doc.canUndo();
    cad::BodyDiagnostics result;
    require(doc.diagnoseBody(id,&result,&error),("Diagnostics query failed: "+error.toStdString()).c_str());
    require(result.valid,"Box diagnostics marked a valid box invalid");
    require(result.solids==1&&result.shells==1,"Box solid/shell counts were incorrect");
    require(result.faces==6&&result.wires==6&&result.edges==12,
            "Box topology counts were incorrect");
    require(result.vertices==8,"Box diagnostics did not count unique vertices");
    closeTo(result.volume,1680);closeTo(result.surfaceArea,856);
    closeTo(result.minX,0);closeTo(result.maxX,10);closeTo(result.minY,0);closeTo(result.maxY,12);
    closeTo(result.minZ,0);closeTo(result.maxZ,14);
    closeTo(result.centerX,5);closeTo(result.centerY,6);closeTo(result.centerZ,7);
    require(!doc.diagnoseBody("missing",&result,&error),"Diagnostics accepted a missing body ID");
    require(!doc.diagnoseBody(id,nullptr,&error),"Diagnostics accepted a null output");
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Read-only diagnostics changed scene history");
}
void testDensityMassProperties() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create density-mass test box");
    const QString id=doc.parts().first().id;
    const bool undoBefore=doc.canUndo();
    cad::DensityMassProperties result;
    require(doc.measureMassFromDensity(id,7850,&result,&error),"Mass calculation failed");
    closeTo(result.volume,1680);closeTo(result.densityKgPerM3,7850);closeTo(result.massKg,0.013188,1e-10);
    closeTo(result.centerX,5);closeTo(result.centerY,6);closeTo(result.centerZ,7);
    require(!doc.measureMassFromDensity(id,0,&result,&error),"Zero density was accepted");
    require(!doc.measureMassFromDensity(id,std::numeric_limits<double>::infinity(),&result,&error),"Infinite density was accepted");
    require(!doc.measureMassFromDensity(id,1000001,&result,&error),"Out-of-range density was accepted");
    require(!doc.measureMassFromDensity("missing",7850,&result,&error),"Missing body was accepted");
    require(!doc.measureMassFromDensity(id,7850,nullptr,&error),"Null mass-properties output was accepted");
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Mass query mutated scene history");
}
void testBodyScheduleCsv() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create CSV schedule box");
    require(doc.addPrimitive(cad::Primitive::Sphere,{3},&error),"Could not create CSV schedule sphere");
    const QString firstId=doc.parts().first().id;
    require(doc.rename(firstId,"Base, \"upper\" plate",&error),"Could not rename CSV schedule test body");
    require(doc.setVisibility(doc.parts().last().id,false,&error),"Could not hide CSV schedule sphere");
    QTemporaryDir dir;require(dir.isValid(),"Could not create CSV temporary folder");
    const QString path=dir.filePath("body_schedule.csv");
    require(doc.exportBodyScheduleCsv(path,&error),("Body schedule export failed: "+error.toStdString()).c_str());
    const QByteArray csv=readBytes(path);
    require(csv.startsWith("Body ID,Name,Visible,Volume (mm^3),Surface area (mm^2),"),"CSV schedule header is incorrect");
    require(csv.contains("\"Base, \"\"upper\"\" plate\""),"CSV schedule did not quote comma/quote characters correctly");
    require(csv.contains(",true,1680,856,"),"CSV schedule omitted box volume or surface area");
    require(csv.contains(",false,"),"CSV schedule omitted hidden-body visibility");
    require(csv.count('\n')==3,"CSV schedule should contain one header and two body rows");
    require(!doc.exportBodyScheduleCsv({},&error),"Empty CSV path was accepted");
    require(!cad::SceneDocument{}.exportBodyScheduleCsv(path,&error),"Empty document produced a CSV schedule");
    QFile asDirectory(dir.path());
    require(!doc.exportBodyScheduleCsv(dir.path(),&error),"Directory path was accepted as a CSV output file");
}
void testBomPartsStandards() {
    QString error; cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"BOM source creation failed");
    const QString source=doc.parts().first().id;
    require(doc.setMaterial(source,"Aluminum 6061",2700,&error),"BOM material assignment failed");
    require(doc.setPartMetadata(source," plate-001 "," Base plate, upper "," a ","Vehicle\\Front Axle","Purchased"," ACME Supply "," ACM-120 ",&error),"Part metadata assignment failed");
    require(doc.find(source)->partNumber=="PLATE-001"&&doc.find(source)->description=="Base plate, upper"&&doc.find(source)->revision=="A",
            "Part metadata was not normalized");
    require(doc.find(source)->assemblyPath=="Vehicle/Front Axle","Assembly path was not normalized into hierarchical segments");
    require(doc.find(source)->procurementType=="Purchased"&&doc.find(source)->supplier=="ACME Supply"&&doc.find(source)->supplierPartNumber=="ACM-120",
            "Purchased-item supplier fields were not normalized");
    require(!doc.setPartMetadata(source,"PLATE-001","bad\nline","A",&error),"Metadata line break was accepted");
    require(!doc.setPartMetadata(source,"PLATE-001","Base plate, upper","A","Vehicle/../Unsafe",&error),"Assembly path traversal segment was accepted");
    require(!doc.setPartMetadata(source,"PLATE-001","Base plate, upper","A","Vehicle/Front Axle","Other","ACME Supply","ACM-120",&error),
            "Unsupported procurement type was accepted");
    require(!doc.setPartMetadata(source,"PLATE-001","Base plate, upper","A","Vehicle/Front Axle","Purchased",QString(81,'S'),"ACM-120",&error),
            "Overlong supplier name was accepted");
    require(doc.duplicate(source,20,0,0,&error),"BOM duplicate creation failed");
    const QString instance=doc.parts().last().id;
    require(doc.find(instance)->partNumber=="PLATE-001"&&doc.find(instance)->description=="Base plate, upper"&&doc.find(instance)->revision=="A",
            "Duplicate did not inherit part metadata");
    require(doc.find(instance)->assemblyPath=="Vehicle/Front Axle","Duplicate did not inherit its assembly path");
    require(doc.find(instance)->procurementType=="Purchased"&&doc.find(instance)->supplier=="ACME Supply"&&doc.find(instance)->supplierPartNumber=="ACM-120",
            "Duplicate did not inherit supplier procurement data");
    require(doc.find(instance)->materialName=="Aluminum 6061","Duplicate did not preserve assigned material");

    // Two legal metadata combinations must not collide at field separators.
    cad::SceneDocument separatorDoc;
    require(separatorDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),
            "Separator-collision source creation failed");
    const QString separatorFirst=separatorDoc.parts().first().id;
    require(separatorDoc.duplicate(separatorFirst,20,0,0,&error),
            "Separator-collision duplicate creation failed");
    const QString separatorSecond=separatorDoc.parts().last().id;
    require(separatorDoc.setPartMetadata(separatorFirst,"KEY-001","X|Y","Z",&error) &&
            separatorDoc.setPartMetadata(separatorSecond,"KEY-001","X","Y|Z",&error),
            "Separator-collision metadata setup failed");
    const auto separateRows=separatorDoc.billOfMaterials(&error);
    require(separateRows.size()==2 && separateRows[0].quantity==1 && separateRows[1].quantity==1,
            "BOM incorrectly grouped distinct metadata containing field separators");
    require(separatorDoc.setPartMetadata(separatorSecond,"KEY-001","X|Y","Z",&error),
            "Matching separator metadata setup failed");
    const auto mergedRows=separatorDoc.billOfMaterials(&error);
    require(mergedRows.size()==1 && mergedRows.first().quantity==2,
            "BOM failed to group truly matching metadata containing field separators");
    // Procurement fields are independent identity fields, even with separators.
    require(separatorDoc.setPartMetadata(separatorFirst,"KEY-001","Same","A","",
                                         "Purchased","North|West","SKU-7",&error) &&
            separatorDoc.setPartMetadata(separatorSecond,"KEY-001","Same","A","",
                                         "Purchased","North","West|SKU-7",&error),
            "Supplier separator-collision metadata setup failed");
    const auto supplierRows=separatorDoc.billOfMaterials(&error);
    require(supplierRows.size()==2 && supplierRows[0].quantity==1 && supplierRows[1].quantity==1,
            "BOM merged distinct supplier and catalogue fields containing separators");
    require(separatorDoc.setPartMetadata(separatorSecond,"KEY-001","Same","A","",
                                         "Purchased","North|West","SKU-7",&error),
            "Matching supplier separator metadata setup failed");
    const auto supplierMergedRows=separatorDoc.billOfMaterials(&error);
    require(supplierMergedRows.size()==1 && supplierMergedRows.first().quantity==2,
            "BOM failed to group matching supplier fields containing separators");

    // Length prefixes must count QString code units consistently for Unicode fields.
    require(separatorDoc.setPartMetadata(separatorFirst,"KEY-001",QString::fromUtf8("Café|A"),"B",&error) &&
            separatorDoc.setPartMetadata(separatorSecond,"KEY-001",QString::fromUtf8("Café"),"A|B",&error),
            "Unicode separator-collision metadata setup failed");
    const auto unicodeRows=separatorDoc.billOfMaterials(&error);
    require(unicodeRows.size()==2 && unicodeRows[0].quantity==1 && unicodeRows[1].quantity==1,
            "BOM merged distinct Unicode metadata with separators");
    // Verify the derived BOM remains distinct in export and after native save/reload.
    QTemporaryDir separatorDir;
    require(separatorDir.isValid(),"Separator-collision temporary directory failed");
    const QString separatorCsvPath=separatorDir.filePath("separator-bom.csv");
    require(separatorDoc.exportBillOfMaterialsCsv(separatorCsvPath,&error),
            "Separator-collision BOM CSV export failed");
    const QByteArray separatorCsv=readBytes(separatorCsvPath);
    require(separatorCsv.count('\n')==3 &&
            separatorCsv.contains(QString::fromUtf8("\"Café|A\"").toUtf8()) &&
            separatorCsv.contains(QString::fromUtf8("\"Café\"").toUtf8()),
            "Separator-collision BOM CSV lost rows or Unicode descriptions");
    const QString separatorScenePath=separatorDir.filePath("separator.cadscene");
    require(separatorDoc.saveAs(separatorScenePath,&error),
            "Separator-collision scene save failed");
    cad::SceneDocument restoredSeparatorDoc;
    require(restoredSeparatorDoc.load(separatorScenePath,&error),
            "Separator-collision scene reload failed");
    const auto restoredSeparatorRows=restoredSeparatorDoc.billOfMaterials(&error);
    require(restoredSeparatorRows.size()==2 &&
            restoredSeparatorRows[0].quantity==1 && restoredSeparatorRows[1].quantity==1,
            "Separator-collision BOM incorrectly grouped rows after scene reload");
    require(restoredSeparatorDoc.find(separatorFirst) && restoredSeparatorDoc.find(separatorSecond) &&
            restoredSeparatorDoc.find(separatorFirst)->description==QString::fromUtf8("Café|A") &&
            restoredSeparatorDoc.find(separatorSecond)->revision=="A|B",
            "Separator-collision metadata did not persist through scene reload");

    // Supplier catalogue numbers retain case; distinct SKUs must not merge in the BOM.
    cad::SceneDocument skuDoc;
    require(skuDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),
            "Case-sensitive SKU test source creation failed");
    const QString skuFirst=skuDoc.parts().first().id;
    require(skuDoc.duplicate(skuFirst,20,0,0,&error),
            "Case-sensitive SKU test duplicate creation failed");
    const QString skuSecond=skuDoc.parts().last().id;
    require(skuDoc.setPartMetadata(skuFirst,"SKU-001","Purchased plate","A","",
                                   "Purchased","Parts Vendor","ab-17",&error) &&
            skuDoc.setPartMetadata(skuSecond,"SKU-001","Purchased plate","A","",
                                   "Purchased","Parts Vendor","AB-17",&error),
            "Case-sensitive SKU metadata setup failed");
    require(skuDoc.find(skuFirst)->supplierPartNumber=="ab-17" &&
            skuDoc.find(skuSecond)->supplierPartNumber=="AB-17",
            "Supplier catalogue numbers were unexpectedly case-normalized");
    const auto skuRows=skuDoc.billOfMaterials(&error);
    require(skuRows.size()==2 && skuRows[0].quantity==1 && skuRows[1].quantity==1,
            "BOM merged different case-sensitive supplier catalogue numbers");
    const auto skuCheck=skuDoc.checkStandards(cad::StandardsProfile::BomData);
    require(std::any_of(skuCheck.findings.cbegin(),skuCheck.findings.cend(),[](const auto& finding) {
                return finding.code=="PART_NUMBER_CONFLICT";
            }), "BOM checker did not detect conflicting supplier catalogue numbers");
    QTemporaryDir skuDir;
    require(skuDir.isValid(),"Case-sensitive SKU test temporary directory failed");
    const QString skuCsvPath=skuDir.filePath("case-sensitive-skus.csv");
    require(skuDoc.exportBillOfMaterialsCsv(skuCsvPath,&error),
            "Case-sensitive SKU BOM export failed");
    const QByteArray skuCsv=readBytes(skuCsvPath);
    require(skuCsv.count('\n')==3 && skuCsv.contains("\"ab-17\"") && skuCsv.contains("\"AB-17\""),
            "Case-sensitive supplier catalogue numbers were lost or grouped in the BOM CSV");
    const QString skuScenePath=skuDir.filePath("case-sensitive-skus.cadscene");
    require(skuDoc.saveAs(skuScenePath,&error),"Case-sensitive SKU scene save failed");
    cad::SceneDocument loadedSkuDoc;
    require(loadedSkuDoc.load(skuScenePath,&error),"Case-sensitive SKU scene reopen failed");
    require(loadedSkuDoc.find(skuFirst) && loadedSkuDoc.find(skuSecond) &&
            loadedSkuDoc.find(skuFirst)->supplierPartNumber=="ab-17" &&
            loadedSkuDoc.find(skuSecond)->supplierPartNumber=="AB-17" &&
            loadedSkuDoc.billOfMaterials(&error).size()==2,
            "Case-sensitive supplier catalogue numbers were lost or grouped after scene reopen");
    require(skuDoc.setPartMetadata(skuSecond,"SKU-001","Purchased plate","A","",
                                   "Purchased","Parts Vendor","ab-17",&error),
            "Matching case-sensitive SKU metadata setup failed");
    const auto identicalSkuRows=skuDoc.billOfMaterials(&error);
    require(identicalSkuRows.size()==1 && identicalSkuRows.first().quantity==2,
            "Identical supplier catalogue numbers did not group into one BOM row");

    // Editable description, material and supplier spelling is preserved in reports.
    // The BOM checker already treats differences in these fields as conflicting.
    cad::SceneDocument caseDoc;
    require(caseDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),
            "Case-sensitive BOM metadata source creation failed");
    const QString caseFirst=caseDoc.parts().first().id;
    require(caseDoc.duplicate(caseFirst,20,0,0,&error),
            "Case-sensitive BOM metadata duplicate failed");
    const QString caseSecond=caseDoc.parts().last().id;
    require(caseDoc.setPartMetadata(caseFirst,"CASE-001","Base Plate","A","",
                                    "Purchased","Fabricator","FG-1",&error) &&
            caseDoc.setPartMetadata(caseSecond,"CASE-001","base plate","A","",
                                    "Purchased","Fabricator","FG-1",&error),
            "Description spelling fixtures failed");
    require(caseDoc.setMaterial(QStringList{caseFirst,caseSecond},"Alloy One",7850,&error),
            "Case-sensitive material fixture failed");
    require(caseDoc.billOfMaterials(&error).size()==2,
            "BOM merged distinct description capitalization");
    const auto caseCheck=caseDoc.checkStandards(cad::StandardsProfile::BomData);
    require(std::any_of(caseCheck.findings.cbegin(),caseCheck.findings.cend(),
                        [](const auto& finding) { return finding.code=="PART_NUMBER_CONFLICT"; }),
            "Description case conflict was not detected");
    require(caseDoc.setPartMetadata(caseSecond,"CASE-001","Base Plate","A","",
                                    "Purchased","Fabricator","FG-1",&error),
            "Matching description spelling fixture failed");
    require(caseDoc.billOfMaterials(&error).size()==1,
            "BOM failed to group matching descriptions");
    require(caseDoc.setMaterial(caseSecond,"alloy one",7850,&error),
            "Material capitalization fixture failed");
    require(caseDoc.billOfMaterials(&error).size()==2,
            "BOM merged differently capitalized material names");
    require(caseDoc.setMaterial(caseSecond,"Alloy One",7850,&error),
            "Matching material spelling fixture failed");
    require(caseDoc.billOfMaterials(&error).size()==1,
            "BOM failed to group matching material names");
    require(caseDoc.setPartMetadata(caseSecond,"CASE-001","Base Plate","A","",
                                    "Purchased","fabricator","FG-1",&error),
            "Supplier capitalization fixture failed");
    const auto supplierCaseRows=caseDoc.billOfMaterials(&error);
    require(supplierCaseRows.size()==2 && supplierCaseRows[0].quantity==1 && supplierCaseRows[1].quantity==1,
            "BOM merged differently capitalized supplier names");
    QTemporaryDir caseDir;require(caseDir.isValid(),"Case-sensitive metadata temporary folder failed");
    const QString caseCsvPath=caseDir.filePath("case-metadata-bom.csv");
    require(caseDoc.exportBillOfMaterialsCsv(caseCsvPath,&error),"Case-sensitive metadata BOM export failed");
    const QByteArray caseCsv=readBytes(caseCsvPath);
    require(caseCsv.count('\n')==3 && caseCsv.contains("\"Fabricator\"") && caseCsv.contains("\"fabricator\""),
            "BOM CSV collapsed supplier capitalization");
    require(caseDoc.setPartMetadata(caseSecond,"CASE-001","Base Plate","A","",
                                    "Purchased","Fabricator","FG-1",&error),
            "Matching supplier spelling fixture failed");
    require(caseDoc.billOfMaterials(&error).size()==1,
            "BOM failed to group matching supplier names");

    // Distinct valid densities can round to the same 15-digit CSV number.
    cad::SceneDocument densityDoc;
    require(densityDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),
            "Density identity test source creation failed");
    const QString densityFirst=densityDoc.parts().first().id;
    require(densityDoc.duplicate(densityFirst,20,0,0,&error),
            "Density identity test duplicate failed");
    const QString densitySecond=densityDoc.parts().last().id;
    require(densityDoc.setPartMetadata(densityFirst,"DENS-001","Density test","A",&error) &&
            densityDoc.setPartMetadata(densitySecond,"DENS-001","Density test","A",&error),
            "Density identity metadata fixture failed");
    const double densityBase=2700.0;
    const double densityAdjacent=std::nextafter(densityBase,std::numeric_limits<double>::infinity());
    require(densityAdjacent>densityBase &&
            QString::number(densityAdjacent,'g',15)==QString::number(densityBase,'g',15),
            "Adjacent densities do not reproduce the 15-digit collision fixture");
    require(densityDoc.setMaterial(densityFirst,"Alloy",densityBase,&error) &&
            densityDoc.setMaterial(densitySecond,"Alloy",densityAdjacent,&error),
            "Density identity material fixture failed");
    const auto distinctDensityRows=densityDoc.billOfMaterials(&error);
    require(distinctDensityRows.size()==2 &&
            distinctDensityRows[0].quantity==1 && distinctDensityRows[1].quantity==1,
            "BOM grouped distinct representable material densities");
    const auto densityCheck=densityDoc.checkStandards(cad::StandardsProfile::BomData);
    require(std::any_of(densityCheck.findings.cbegin(),densityCheck.findings.cend(),
                        [](const auto& finding) { return finding.code=="PART_NUMBER_CONFLICT"; }),
            "Density conflict checker did not distinguish representable densities");
    QTemporaryDir densityDir;require(densityDir.isValid(),"Density identity temporary folder failed");
    const QString densityScene=densityDir.filePath("density-identity.cadscene");
    require(densityDoc.saveAs(densityScene,&error),"Density identity scene save failed");
    cad::SceneDocument restoredDensity;
    require(restoredDensity.load(densityScene,&error),"Density identity scene reopen failed");
    require(restoredDensity.find(densitySecond) &&
            restoredDensity.find(densitySecond)->materialDensityKgPerM3==densityAdjacent &&
            restoredDensity.billOfMaterials(&error).size()==2,
            "Density identity lost a representable density during save/reopen");
    require(densityDoc.setMaterial(densitySecond,"Alloy",densityBase,&error),
            "Matching density fixture failed");
    const auto matchingDensityRows=densityDoc.billOfMaterials(&error);
    require(matchingDensityRows.size()==1 && matchingDensityRows.first().quantity==2,
            "BOM failed to group exactly matching material densities");

    // The full-project report must handle more than the 50-body drawing source limit.
    cad::SceneDocument largeDoc;
    require(largeDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),
            "Full BOM body-limit fixture creation failed");
    const QString largeFirst=largeDoc.parts().first().id;
    require(largeDoc.setPartMetadata(largeFirst,"BULK-001","Repeated fitting","A",&error),
            "Full BOM body-limit metadata fixture failed");
    require(largeDoc.linearPatternAlongVector(largeFirst,1,0,0,51,25,&error),
            "Full BOM body-limit pattern creation failed");
    require(largeDoc.parts().size()==51,"Full BOM body-limit fixture did not produce 51 bodies");
    const auto largeRows=largeDoc.billOfMaterials(&error);
    int largeQuantity=0;
    for(const auto& row:largeRows)largeQuantity+=row.quantity;
    require(!largeRows.isEmpty() && largeQuantity==51,
            "Full-document BOM omitted bodies beyond the 50-body drawing source cap");
    QStringList largeIds;for(const auto& part:largeDoc.parts())largeIds.append(part.id);
    const auto scopedFifty=largeDoc.billOfMaterials(largeIds.mid(0,50),&error);
    int scopedQuantity=0;
    for(const auto& row:scopedFifty)scopedQuantity+=row.quantity;
    require(!scopedFifty.isEmpty() && scopedQuantity==50,
            "Drawing source-scoped BOM rejected 50 valid bodies");
    error.clear();
    require(largeDoc.billOfMaterials(largeIds,&error).isEmpty() &&
            error.contains("fifty"),
            "Drawing source-scoped BOM accepted more than 50 bodies");
    QTemporaryDir largeDir;require(largeDir.isValid(),"Full BOM body-limit temporary folder failed");
    const QString largeCsvPath=largeDir.filePath("full-bom.csv");
    require(largeDoc.exportBillOfMaterialsCsv(largeCsvPath,&error),
            "Full BOM CSV export failed for 51 bodies");
    const QByteArray largeCsv=readBytes(largeCsvPath);
    require(largeCsv.startsWith("Item,Part number,") && largeCsv.count('\n')==largeRows.size()+1,
            "Full BOM CSV omitted rows beyond the drawing limit");
    const QString largeScene=largeDir.filePath("51-bodies.cadscene");
    require(largeDoc.saveAs(largeScene,&error),"51-body scene save failed");
    cad::SceneDocument reloadedLarge;
    require(reloadedLarge.load(largeScene,&error),"51-body scene reopen failed");
    const auto reloadedLargeRows=reloadedLarge.billOfMaterials(&error);
    int reloadedQuantity=0;
    for(const auto& row:reloadedLargeRows)reloadedQuantity+=row.quantity;
    require(!reloadedLargeRows.isEmpty() && reloadedQuantity==51,
            "Full BOM body count changed after native scene reload");

    const auto bom=doc.billOfMaterials(&error);
    require(bom.size()==1&&bom.first().quantity==2,"Identical metadata and geometry did not aggregate into quantity two");
    closeTo(bom.first().unitVolumeMm3,1680);closeTo(bom.first().totalVolumeMm3,3360);
    closeTo(bom.first().unitMassKg,0.004536,1e-10);closeTo(bom.first().totalMassKg,0.009072,1e-10);
    const auto list=doc.partsList(&error);
    require(list.size()==2&&list[0].itemNumber==1&&list[1].itemNumber==2,"Parts list did not emit one stable row per body");
    require(list[0].bodyId!=list[1].bodyId&&list[0].quantity==1,"Parts list rows did not retain body identity and unit quantity");

    QTemporaryDir dir;require(dir.isValid(),"BOM test temporary directory failed");
    const QString bomPath=dir.filePath("bom.csv"),listPath=dir.filePath("parts.csv");
    require(doc.exportBillOfMaterialsCsv(bomPath,&error),"BOM export failed");
    require(doc.exportPartsListCsv(listPath,&error),"Parts-list export failed");
    const QByteArray bomCsv=readBytes(bomPath),listCsv=readBytes(listPath);
    require(bomCsv.startsWith("Item,Part number,Description,Revision,Material,Quantity,"),"BOM header missing required columns");
    require(bomCsv.contains("\"Base plate, upper\"")&&bomCsv.contains(",2,1680,3360,"),"BOM CSV lost escaped text or aggregated quantities");
    require(listCsv.startsWith("Item,Body ID,Body name,Part number,Description,Revision,Material,Quantity,"),"Parts-list header missing required columns");
    require(listCsv.count('\n')==3,"Parts list should contain exactly one row for each of two bodies");
    require(!doc.exportBillOfMaterialsCsv({},&error)&&!doc.exportPartsListCsv(dir.path(),&error),"Invalid CSV destinations were accepted");
    require(bomCsv.contains("Vehicle/Front Axle")&&listCsv.contains("Vehicle/Front Axle"),"CSV exports omitted assembly paths");
    require(bomCsv.contains("Purchased")&&bomCsv.contains("ACME Supply")&&bomCsv.contains("ACM-120")&&
            listCsv.contains("ACME Supply")&&listCsv.contains("ACM-120"),"CSV exports omitted purchased-item supplier data");

    const auto bomCheck=doc.checkStandards(cad::StandardsProfile::BomData);
    require(bomCheck.passed()&&bomCheck.errors==0&&bomCheck.warnings==0,"Complete BOM data unexpectedly failed its checker");
    const auto isoCheck=doc.checkStandards(cad::StandardsProfile::IsoDrawingReadiness);
    const auto asmeCheck=doc.checkStandards(cad::StandardsProfile::AsmeDrawingReadiness);
    require(isoCheck.profile==cad::StandardsProfile::IsoDrawingReadiness&&isoCheck.passed(),"ISO pre-drawing readiness report failed unexpectedly");
    require(asmeCheck.profile==cad::StandardsProfile::AsmeDrawingReadiness&&asmeCheck.passed(),"ASME pre-drawing readiness report failed unexpectedly");
    require(std::any_of(isoCheck.findings.cbegin(),isoCheck.findings.cend(),[](const auto& f){return f.code=="CHECK_SCOPE"&&f.severity==cad::FindingSeverity::Info;}),
            "ISO checker omitted its scope limitations");
    require(std::any_of(isoCheck.findings.cbegin(),isoCheck.findings.cend(),[](const auto& f){return f.message.contains("formal ISO clauses/tolerances are not certified");}),
            "ISO checker did not explain its formal-compliance boundary");
    require(std::any_of(asmeCheck.findings.cbegin(),asmeCheck.findings.cend(),[](const auto& f){return f.message.contains("formal Y14 dimensioning");}),
            "ASME checker did not explain its formal-compliance boundary");

    require(doc.setPartMetadata(instance,"PLATE-001","Different description","A",&error),"Conflicting part metadata setup failed");
    const auto conflict=doc.checkStandards(cad::StandardsProfile::BomData);
    require(!conflict.passed()&&conflict.errors==1,"Duplicate part number with conflicting metadata was not rejected");
    const auto separateBom=doc.billOfMaterials(&error);
    require(separateBom.size()==2,"BOM incorrectly aggregated conflicting part definitions");
    require(doc.undo(),"Part metadata undo failed");

    require(doc.setPartMetadata(instance,"PLATE-001","Base plate, upper","A","Vehicle/Rear Axle",&error),"Assembly assignment failed");
    const auto splitAssemblies=doc.billOfMaterials(&error);
    require(splitAssemblies.size()==2&&splitAssemblies[0].quantity==1&&splitAssemblies[1].quantity==1,
            "Identical parts in different assembly paths were incorrectly merged");
    require(splitAssemblies[0].assemblyPath!=splitAssemblies[1].assemblyPath,"Assembly-separated BOM rows lost their paths");
    require(doc.setPartMetadata(instance,"PLATE-001","Base plate, upper","A","Vehicle/Front Axle",&error),"Assembly path restore failed");
    require(doc.setPartMetadata(instance,"PLATE-001","Base plate, upper","A","Vehicle/Front Axle","Purchased","Second Supplier","ACM-120",&error),
            "Alternate supplier assignment failed");
    const auto alternateSupplierBom=doc.billOfMaterials(&error);
    require(alternateSupplierBom.size()==2&&alternateSupplierBom[0].supplier!=alternateSupplierBom[1].supplier,
            "BOM incorrectly merged purchased parts with different suppliers");
    require(doc.setPartMetadata(instance,"PLATE-001","Base plate, upper","A","Vehicle/Front Axle","Purchased","ACME Supply","ACM-120",&error),
            "Supplier restore failed");

    const QString scene=dir.filePath("bom.cadscene");
    cad::DrawingReadinessPolicy drawingPolicy;
    drawingPolicy.requiredTitleBlockFields={"drawing_number","revision","material"};
    drawingPolicy.requiredAnnotationConventions={"envelope_dimensions","general_notes"};
    require(doc.setDrawingReadinessPolicy(drawingPolicy,&error),"Valid project drawing policy was rejected");
    require(doc.dirty(),"Changing project drawing requirements did not mark the scene as modified");
    cad::DrawingReadinessPolicy invalidPolicy=drawingPolicy;invalidPolicy.requiredTitleBlockFields.append("drawing_number");
    require(!doc.setDrawingReadinessPolicy(invalidPolicy,&error),"Duplicate project drawing requirements were accepted");
    invalidPolicy=drawingPolicy;invalidPolicy.requiredAnnotationConventions.append("invented_convention");
    require(!doc.setDrawingReadinessPolicy(invalidPolicy,&error),"Unsupported project annotation convention was accepted");
    require(doc.undo()&&doc.drawingReadinessPolicy().requiredTitleBlockFields.isEmpty(),"Project drawing requirements did not undo");
    require(doc.redo()&&doc.drawingReadinessPolicy().requiredTitleBlockFields==drawingPolicy.requiredTitleBlockFields&&
            doc.drawingReadinessPolicy().requiredAnnotationConventions==drawingPolicy.requiredAnnotationConventions,
            "Project drawing requirements did not redo");
    require(doc.saveAs(scene,&error),"BOM metadata save failed");
    const QJsonObject root=QJsonDocument::fromJson(readBytes(scene)).object();
    require(root.value("schema").toString()=="cadworkbench.scene.v22","New scenes did not use schema v22");
    cad::SceneDocument drawingXmlFixture;
    QString drawingXmlError;
    require(drawingXmlFixture.addPrimitive(cad::Primitive::Box,{5,6,7},&drawingXmlError),
            "Could not create persistent drawing XML fixture");
    const QString drawingXmlBody=drawingXmlFixture.parts().first().id;
    cad::DrawingSheetSettings drawingXmlSettings;
    drawingXmlSettings.sourceBodyIds={drawingXmlBody};
    drawingXmlSettings.balloonAnnotations={{"valid-balloon",drawingXmlBody,250,170}};
    require(drawingXmlFixture.setDrawingSheetSettings(drawingXmlBody,drawingXmlSettings,&drawingXmlError),
            "Could not save persistent drawing XML fixture");
    const QString drawingXmlScene=dir.filePath("drawing-xml-fixture.cadscene");
    require(drawingXmlFixture.saveAs(drawingXmlScene,&drawingXmlError),
            "Could not serialize persistent drawing XML fixture");
    QJsonObject invalidXmlRoot=QJsonDocument::fromJson(readBytes(drawingXmlScene)).object();
    QJsonArray invalidXmlParts=invalidXmlRoot.value("parts").toArray();
    for(int i=0;i<invalidXmlParts.size();++i){
        QJsonObject record=invalidXmlParts[i].toObject();
        if(!record.value("drawing_settings").isObject())continue;
        QJsonObject sheet=record.value("drawing_settings").toObject();
        QJsonArray balloons=sheet.value("balloon_annotations").toArray();
        if(balloons.isEmpty())continue;
        QJsonObject balloon=balloons.first().toObject();
        balloon.insert("id",QStringLiteral("bad")+QString(QChar(0x0001)));
        balloons[0]=balloon;sheet.insert("balloon_annotations",balloons);
        record.insert("drawing_settings",sheet);invalidXmlParts[i]=record;
        break;
    }
    invalidXmlRoot.insert("parts",invalidXmlParts);
    const QString invalidXmlPath=dir.filePath("invalid-drawing-xml.cadscene");
    writeBytes(invalidXmlPath,QJsonDocument(invalidXmlRoot).toJson());
    cad::SceneDocument rejectedXml;error.clear();
    require(!rejectedXml.load(invalidXmlPath,&error),
            "Loader accepted a persisted BOM balloon ID with XML-forbidden control characters");
    cad::SceneDocument loaded;require(loaded.load(scene,&error),"BOM metadata load failed");
    require(loaded.drawingReadinessPolicy().requiredTitleBlockFields==drawingPolicy.requiredTitleBlockFields&&
            loaded.drawingReadinessPolicy().requiredAnnotationConventions==drawingPolicy.requiredAnnotationConventions,
            "Project drawing requirements did not persist through save/reopen");
    QJsonObject schema12Root=root;schema12Root.insert("schema","cadworkbench.scene.v12");schema12Root.remove("drawing_readiness_policy");
    const QString schema12Scene=dir.filePath("bom-v12.cadscene");writeBytes(schema12Scene,QJsonDocument(schema12Root).toJson());
    cad::SceneDocument migratedPolicy;require(migratedPolicy.load(schema12Scene,&error),"Schema-v12 project did not migrate to default drawing requirements");
    require(migratedPolicy.drawingReadinessPolicy().requiredTitleBlockFields.isEmpty()&&
            migratedPolicy.drawingReadinessPolicy().requiredAnnotationConventions.isEmpty(),
            "Schema-v12 migration did not use default drawing requirements");
    QJsonObject invalidPolicyRoot=root;QJsonObject invalidPolicyObject=invalidPolicyRoot.value("drawing_readiness_policy").toObject();
    invalidPolicyObject.insert("required_title_block_fields",QJsonArray{"unknown_field"});invalidPolicyRoot.insert("drawing_readiness_policy",invalidPolicyObject);
    const QString invalidPolicyScene=dir.filePath("invalid-drawing-policy.cadscene");writeBytes(invalidPolicyScene,QJsonDocument(invalidPolicyRoot).toJson());
    require(!loaded.load(invalidPolicyScene,&error)&&loaded.drawingReadinessPolicy().requiredTitleBlockFields==drawingPolicy.requiredTitleBlockFields,
            "Invalid project checklist load was accepted or changed existing readiness settings");
    require(loaded.find(source)->partNumber=="PLATE-001"&&loaded.find(source)->description=="Base plate, upper"&&loaded.find(source)->revision=="A",
            "Part metadata did not round-trip through save/reopen");
    require(loaded.find(source)->assemblyPath=="Vehicle/Front Axle","Assembly path did not round-trip through save/reopen");
    require(loaded.find(source)->procurementType=="Purchased"&&loaded.find(source)->supplier=="ACME Supply"&&loaded.find(source)->supplierPartNumber=="ACM-120",
            "Purchased supplier information did not round-trip through save/reopen");
    require(loaded.billOfMaterials(&error).first().quantity==2,"Saved BOM quantity changed after reload");

    QJsonObject schema4Root=root;
    schema4Root.insert("schema","cadworkbench.scene.v4");
    QJsonArray schema4Parts=schema4Root.value("parts").toArray();
    for (int i=0;i<schema4Parts.size();++i) {
        QJsonObject part=schema4Parts[i].toObject();
        part.remove("procurement_type");part.remove("supplier");part.remove("supplier_part_number");
        schema4Parts[i]=part;
    }
    schema4Root.insert("parts",schema4Parts);
    const QString schema4Scene=dir.filePath("bom-v4.cadscene");
    QFile schema4File(schema4Scene);require(schema4File.open(QIODevice::WriteOnly),"Could not create schema-v4 migration fixture");
    require(schema4File.write(QJsonDocument(schema4Root).toJson())>0,"Could not write schema-v4 migration fixture");schema4File.close();
    cad::SceneDocument migrated;require(migrated.load(schema4Scene,&error),"Schema-v4 scene failed to migrate");
    require(migrated.find(source)->assemblyPath=="Vehicle/Front Axle"&&migrated.find(source)->procurementType=="Manufactured"&&
            migrated.find(source)->supplier.isEmpty()&&migrated.find(source)->supplierPartNumber.isEmpty(),
            "Schema-v4 migration did not preserve assembly path or default procurement fields");

    cad::SceneDocument incomplete;require(incomplete.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Incomplete standards fixture failed");
    const auto incompleteReport=incomplete.checkStandards(cad::StandardsProfile::BomData);
    require(incompleteReport.passed()&&incompleteReport.warnings>=4,"Missing BOM fields were not reported as actionable warnings");
    const QString incompleteId=incomplete.parts().first().id;
    require(incomplete.setPartMetadata(incompleteId,"PUR-001","Purchased fitting","A",{},"Purchased",{}, {},&error),
            "Purchased-item completeness fixture setup failed");
    const auto incompletePurchased=incomplete.checkStandards(cad::StandardsProfile::BomData);
    require(incompletePurchased.passed()&&std::any_of(incompletePurchased.findings.cbegin(),incompletePurchased.findings.cend(),[](const auto& f){return f.code=="SUPPLIER_MISSING";}),
            "Purchased item without a supplier was not flagged");
    require(std::any_of(incompletePurchased.findings.cbegin(),incompletePurchased.findings.cend(),[](const auto& f){return f.code=="SUPPLIER_PART_NUMBER_MISSING";}),
            "Purchased item without a supplier part number was not flagged");
    require(!cad::SceneDocument{}.checkStandards(cad::StandardsProfile::BomData).passed(),"Empty model passed the standards checker");

    cad::SceneDocument checklist;require(checklist.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Configurable standards fixture failed");
    const QString checklistId=checklist.parts().first().id;
    require(checklist.setPartMetadata(checklistId,"CHK-001","Checklist fixture","A",&error)&&
            checklist.setMaterial(checklistId,"Aluminum",2700,&error),"Could not complete drawing checklist metadata");
    cad::DrawingReadinessPolicy requiredPolicy;requiredPolicy.requiredTitleBlockFields={"drawing_number","revision","material"};
    requiredPolicy.requiredAnnotationConventions={"envelope_dimensions","general_notes","section_view","detail_view",
        "linked_parts_list","bom_balloons"};
    require(checklist.setDrawingReadinessPolicy(requiredPolicy,&error),"Could not apply saved drawing checklist policy");
    auto checklistReport=checklist.checkStandards(cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(checklistReport.findings.cbegin(),checklistReport.findings.cend(),[](const auto& finding){return finding.code=="DRAWING_SHEET_MISSING";}),
            "Configured drawing checklist did not flag a missing saved sheet");
    cad::DrawingSheetSettings checklistSheet;checklistSheet.drawingNumber="CHK-DWG-01";checklistSheet.revision="A";checklistSheet.generalNotes={"Deburr edges"};
    require(checklist.setDrawingSheetSettings(checklistId,checklistSheet,&error),"Could not save the checklist drawing sheet");
    checklistReport=checklist.checkStandards(cad::StandardsProfile::IsoDrawingReadiness);
    const auto missingAnnotation=[&](const QString& key){return std::any_of(checklistReport.findings.cbegin(),checklistReport.findings.cend(),[&](const auto& finding){
        return finding.code=="DRAWING_CONVENTION_MISSING"&&finding.message.contains(key);
    });};
    require(missingAnnotation("section_view")&&missingAnnotation("detail_view")&&missingAnnotation("linked_parts_list")&&missingAnnotation("bom_balloons"),
            "Configured project checklist did not report missing section/detail views, linked parts list, or BOM balloons individually");
    checklistSheet.sectionAnnotations={{"section-check","Y",0.5,240,145}};
    checklistSheet.detailAnnotations={{"detail-check","FRONT",0,0,5,245,80,18}};
    checklistSheet.partsListAnnotation.visible=true;
    checklistSheet.balloonAnnotations={{"balloon-check",checklistId,250,170}};
    require(checklist.setDrawingSheetSettings(checklistId,checklistSheet,&error),"Could not add checklist views and BOM annotations");
    checklistReport=checklist.checkStandards(cad::StandardsProfile::IsoDrawingReadiness);
    require(std::none_of(checklistReport.findings.cbegin(),checklistReport.findings.cend(),[](const auto& finding){
        return finding.code=="DRAWING_SHEET_MISSING"||finding.code=="DRAWING_FIELD_MISSING"||finding.code=="DRAWING_CONVENTION_MISSING";
    }),"A populated saved sheet failed the configured project checklist");
    require(checklist.translate(checklistId,1,0,0,&error),"Could not stale the configured drawing source");
    checklistReport=checklist.checkStandards(cad::StandardsProfile::AsmeDrawingReadiness);
    require(std::any_of(checklistReport.findings.cbegin(),checklistReport.findings.cend(),[](const auto& finding){return finding.code=="DRAWING_SOURCE_STALE";}),
            "ASME project readiness missed a stale saved drawing source");
    require(checklist.refreshDrawingSheetSource(checklistId,&error),"Could not refresh the checklist drawing source revision");
    checklistReport=checklist.checkStandards(cad::StandardsProfile::AsmeDrawingReadiness);
    require(std::none_of(checklistReport.findings.cbegin(),checklistReport.findings.cend(),
                [](const auto& finding){return finding.code=="DRAWING_SOURCE_STALE";}),
            "Refreshing a saved sheet did not clear its project readiness stale-source finding");
}

void testFreeOccurrenceYawPlacement() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,5},&error),
            "Could not create free occurrence yaw fixture");
    const QString source=doc.parts().first().id;
    require(doc.makeComponent(source,"Yaw bracket",&error)&&
            doc.insertComponentOccurrence(source,100,0,0,&error),
            "Could not create reusable yaw test occurrences");
    const QString rotated=doc.parts().last().id;
    require(doc.setFreeOccurrenceYaw(rotated,90,&error),"Could not set free occurrence yaw");
    closeTo(doc.find(rotated)->placementYawDegrees,90);
    cad::BodyDiagnostics diagnostics;
    require(doc.diagnoseBody(rotated,&diagnostics,&error),"Could not inspect yawed occurrence");
    closeTo(diagnostics.minX,80,1e-5);closeTo(diagnostics.maxX,100,1e-5);
    closeTo(diagnostics.minY,0,1e-5);closeTo(diagnostics.maxY,10,1e-5);
    require(doc.undo()&&doc.find(rotated)->placementYawDegrees==0.0,
            "Yaw was not one undoable operation");
    require(doc.redo()&&doc.find(rotated)->placementYawDegrees==90.0,
            "Yaw redo did not restore the recorded orientation");
    require(!doc.setFreeOccurrenceYaw(rotated,36001,&error)&&
            doc.find(rotated)->placementYawDegrees==90.0,
            "Out-of-range yaw changed the occurrence");

    require(!doc.rotate(rotated,cad::Axis::X,10,&error)&&
            !doc.rotateAboutAxis(rotated,0,0,0,1,0,0,10,&error)&&
            doc.find(rotated)->placementYawDegrees==90.0,
            "Unsupported legacy rotations silently invalidated saved yaw");
    require(doc.rotate(rotated,cad::Axis::Z,15,&error)&&
            doc.find(rotated)->placementYawDegrees==105.0,
            "World-Z rotation did not compose the persisted yaw");
    require(doc.undo()&&doc.find(rotated)->placementYawDegrees==90.0,
            "Composed world-Z yaw did not undo cleanly");

    // A new occurrence of the rotated source starts at identity orientation.
    require(doc.insertComponentOccurrence(rotated,200,0,0,&error),
            "Could not insert from a yawed definition source");
    const QString fresh=doc.parts().last().id;
    require(doc.find(fresh)->placementYawDegrees==0.0&&
            doc.diagnoseBody(fresh,&diagnostics,&error),
            "New occurrence inherited the source yaw");
    closeTo(diagnostics.minX,200,1e-5);closeTo(diagnostics.maxX,210,1e-5);
    closeTo(diagnostics.minY,0,1e-5);closeTo(diagnostics.maxY,20,1e-5);

    // Definition synchronization must retain the target's independent yaw.
    require(doc.scale(source,2,&error)&&doc.synchronizeComponentOccurrences(source,&error),
            "Could not synchronize source geometry to yawed occurrences");
    require(doc.find(rotated)->placementYawDegrees==90.0&&
            doc.diagnoseBody(rotated,&diagnostics,&error),
            "Definition synchronization lost target yaw");
    closeTo(diagnostics.minX,60,1e-5);closeTo(diagnostics.maxX,100,1e-5);
    closeTo(diagnostics.minY,0,1e-5);closeTo(diagnostics.maxY,20,1e-5);
    require(doc.duplicate(rotated,70,0,0,&error),"Could not duplicate yawed occurrence");
    const QString duplicate=doc.parts().last().id;
    closeTo(doc.find(duplicate)->placementX,170);
    closeTo(doc.find(duplicate)->placementYawDegrees,90);
    require(doc.diagnoseBody(duplicate,&diagnostics,&error),"Could not inspect yawed duplicate");
    closeTo(diagnostics.minX,130,1e-5);closeTo(diagnostics.maxX,170,1e-5);

    QTemporaryDir dir;require(dir.isValid(),"Could not create yaw persistence directory");
    const QString file=dir.filePath("yaw.cadscene");
    require(doc.saveAs(file,&error),"Could not save yawed occurrence");
    const QJsonObject saved=QJsonDocument::fromJson(readBytes(file)).object();
    require(saved.value("schema").toString()=="cadworkbench.scene.v22",
            "Yawed occurrence scene did not use the latest schema");
    cad::SceneDocument reopened;
    require(reopened.load(file,&error),"Could not reload yawed occurrence");
    closeTo(reopened.find(rotated)->placementYawDegrees,90);
    closeTo(reopened.find(duplicate)->placementYawDegrees,90);
    require(reopened.synchronizeComponentOccurrences(fresh,&error),
            "Could not synchronize yawed instances after reopening");
    require(reopened.diagnoseBody(rotated,&diagnostics,&error),
            "Could not inspect reopened synchronized yawed occurrence");
    closeTo(diagnostics.minX,60,1e-5);closeTo(diagnostics.maxX,100,1e-5);

    QJsonObject legacy=saved;QJsonArray records=legacy.value("parts").toArray();
    for(int i=0;i<records.size();++i){
        QJsonObject record=records[i].toObject();record.remove("placement_yaw_degrees");records[i]=record;
    }
    legacy.insert("parts",records);
    const QString legacyFile=dir.filePath("legacy-v19.cadscene");
    writeBytes(legacyFile,QJsonDocument(legacy).toJson());
    cad::SceneDocument legacyDoc;
    require(legacyDoc.load(legacyFile,&error)&&legacyDoc.find(rotated)->placementYawDegrees==0.0,
            "Old v19 scenes must default missing yaw to zero");
    for(const QJsonValue& invalidYaw:QVector<QJsonValue>{QJsonValue("90"),QJsonValue(36001.0),QJsonValue(QJsonValue::Null)}){
        QJsonObject corrupt=saved;QJsonArray badRecords=corrupt.value("parts").toArray();
        QJsonObject bad=badRecords[1].toObject();bad.insert("placement_yaw_degrees",invalidYaw);
        badRecords[1]=bad;corrupt.insert("parts",badRecords);
        const QString broken=dir.filePath("invalid-yaw.cadscene");
        writeBytes(broken,QJsonDocument(corrupt).toJson());
        require(!reopened.load(broken,&error)&&reopened.find(rotated)->placementYawDegrees==90.0,
                "Invalid persisted yaw was accepted or mutated a loaded document");
    }
    // A free placement operation must not interfere with the existing joint tree.
    cad::SceneDocument joints;
    require(joints.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create free-yaw joint fixture");
    const QString parent=joints.parts().first().id;
    require(joints.makeComponent(parent,"Base",&error)&&
            joints.insertComponentOccurrence(parent,20,0,0,&error),
            "Could not create free-yaw joint instances");
    const QString child=joints.parts().last().id;
    require(joints.addRigidJoint(parent,child,&error),"Could not create rigid joint fixture");
    require(!joints.setFreeOccurrenceYaw(parent,30,&error)&&
            !joints.setFreeOccurrenceYaw(child,30,&error)&&
            joints.find(parent)->placementYawDegrees==0.0&&
            joints.find(child)->placementYawDegrees==0.0,
            "Free occurrence yaw mutated an attached joint tree");
    require(joints.undo()&&joints.find(child)->assemblyJointType.isEmpty(),
            "Rejected yaw introduced an undo checkpoint");

    cad::SceneDocument oriented;
    require(oriented.addPrimitive(cad::Primitive::Box,{10,20,5},&error),
            "Could not create three-axis occurrence fixture");
    const QString orientationSource=oriented.parts().first().id;
    require(oriented.makeComponent(orientationSource,"Oriented bracket",&error)&&
            oriented.insertComponentOccurrence(orientationSource,100,0,0,&error),
            "Could not create three-axis component occurrence");
    const QString orientationId=oriented.parts().last().id;
    require(oriented.setFreeOccurrenceOrientation(orientationId,90,0,0,&error),
            "Could not set free occurrence roll");
    require(oriented.diagnoseBody(orientationId,&diagnostics,&error),
            "Could not inspect rolled occurrence");
    closeTo(diagnostics.minX,100,1e-5);closeTo(diagnostics.maxX,110,1e-5);
    closeTo(diagnostics.minY,-5,1e-5);closeTo(diagnostics.maxY,0,1e-5);
    closeTo(diagnostics.minZ,0,1e-5);closeTo(diagnostics.maxZ,20,1e-5);
    require(oriented.setFreeOccurrenceOrientation(orientationId,90,30,45,&error),
            "Could not combine roll, pitch, and yaw");
    const QString orientationToken=oriented.find(orientationId)->geometryToken;
    require(oriented.setFreeOccurrenceOrientation(orientationId,90,30,45,&error)&&
            oriented.find(orientationId)->geometryToken==orientationToken,
            "Identical occurrence orientation rebuilt the solid");
    require(oriented.setFreeOccurrenceYaw(orientationId,60,&error)&&
            oriented.find(orientationId)->placementRollDegrees==90&&
            oriented.find(orientationId)->placementPitchDegrees==30,
            "Yaw editing erased roll or pitch");
    require(oriented.undo()&&oriented.find(orientationId)->placementYawDegrees==45&&
            oriented.redo()&&oriented.find(orientationId)->placementYawDegrees==60,
            "Three-axis occurrence orientation did not undo and redo");
    require(!oriented.setFreeOccurrenceOrientation(orientationId,0,36001,0,&error)&&
            oriented.find(orientationId)->placementPitchDegrees==30,
            "Invalid pitch changed the occurrence");
    const QString orientedScene=dir.filePath("orientation-v22.cadscene");
    require(oriented.saveAs(orientedScene,&error),"Could not save three-axis occurrence");
    cad::SceneDocument orientedReload;
    require(orientedReload.load(orientedScene,&error)&&
            orientedReload.find(orientationId)->placementRollDegrees==90&&
            orientedReload.find(orientationId)->placementPitchDegrees==30&&
            orientedReload.find(orientationId)->placementYawDegrees==60,
            "Three-axis occurrence orientation did not survive save/reopen");
    require(orientedReload.insertComponentOccurrence(orientationId,200,0,0,&error)&&
            orientedReload.parts().last().placementRollDegrees==0&&
            orientedReload.parts().last().placementPitchDegrees==0,
            "A fresh definition occurrence inherited the source orientation");
    require(orientedReload.linearPatternComponentOccurrences(orientationId,2,0,40,0,&error)&&
            orientedReload.parts().last().placementRollDegrees==90&&
            orientedReload.parts().last().placementPitchDegrees==30&&
            orientedReload.parts().last().placementYawDegrees==60,
            "Linear component pattern lost the source's three-axis orientation");
    require(orientedReload.circularPatternComponentOccurrences(orientationId,2,0,0,0,90,&error)&&
            orientedReload.parts().last().placementRollDegrees==90&&
            orientedReload.parts().last().placementPitchDegrees==30&&
            orientedReload.parts().last().placementYawDegrees==150,
            "Circular component pattern did not compose yaw while preserving roll and pitch");
    require(orientedReload.scale(orientationSource,2,&error)&&
            orientedReload.synchronizeComponentOccurrences(orientationSource,&error)&&
            orientedReload.find(orientationId)->placementRollDegrees==90&&
            orientedReload.find(orientationId)->placementPitchDegrees==30,
            "Definition synchronization lost an occurrence's independent roll and pitch");
    QJsonObject invalidOrientation=QJsonDocument::fromJson(readBytes(orientedScene)).object();
    QJsonArray invalidParts=invalidOrientation.value("parts").toArray();
    QJsonObject invalidRecord=invalidParts[1].toObject();
    invalidRecord.insert("placement_pitch_degrees",QJsonValue("30"));
    invalidParts[1]=invalidRecord;invalidOrientation.insert("parts",invalidParts);
    const QString invalidOrientationPath=dir.filePath("invalid-orientation.cadscene");
    writeBytes(invalidOrientationPath,QJsonDocument(invalidOrientation).toJson());
    require(!orientedReload.load(invalidOrientationPath,&error)&&
            orientedReload.find(orientationId)->placementPitchDegrees==30,
            "Malformed saved pitch was accepted or changed the open scene");
    invalidOrientation.insert("schema","cadworkbench.scene.v21");
    invalidParts=invalidOrientation.value("parts").toArray();invalidRecord=invalidParts[1].toObject();
    invalidRecord.insert("placement_pitch_degrees",30.0);invalidParts[1]=invalidRecord;
    invalidOrientation.insert("parts",invalidParts);
    const QString wrongSchemaPath=dir.filePath("orientation-in-v21.cadscene");
    writeBytes(wrongSchemaPath,QJsonDocument(invalidOrientation).toJson());
    require(!orientedReload.load(wrongSchemaPath,&error),
            "Older schema accepted a three-axis occurrence record");
}

void testFullAssemblyBomBeyondSelectionCap() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create large-assembly BOM fixture");
    const QString source=doc.parts().first().id;
    require(doc.makeComponent(source,"Standard bracket",&error)&&
            doc.setPartMetadata(source,"BRACKET-50","Stock bracket","A","Machine/Frame",
                                "Manufactured","","",&error),
            "Could not create large-assembly reusable component");
    for(int index=1;index<=50;++index) {
        require(doc.insertComponentOccurrence(source,index*15.0,0,0,&error),
                "Could not add the next occurrence to the large-assembly fixture");
    }
    require(doc.parts().size()==51,"Large-assembly fixture has an unexpected body count");
    const auto full=doc.billOfMaterials(&error);
    require(full.size()==1&&full.first().quantity==51&&full.first().bodyIds.size()==51,
            "Full-scene BOM silently failed when the scene exceeded 50 bodies");
    QStringList ids;
    for(const auto& part:doc.parts())ids.append(part.id);
    error.clear();
    require(doc.billOfMaterials(ids,&error).isEmpty()&&error.contains("fifty"),
            "Explicit source-scoped BOM selection accepted more than 50 bodies");
    const auto scoped=doc.billOfMaterials(ids.mid(0,50),&error);
    require(scoped.size()==1&&scoped.first().quantity==50,
            "Valid 50-body source-scoped BOM changed with the full-scene fix");
    QTemporaryDir dir;
    require(dir.isValid(),"Could not create large-assembly BOM export fixture directory");
    const QString csv=dir.filePath("full-assembly.csv");
    require(doc.exportBillOfMaterialsCsv(csv,&error),
            "CSV export could not access the full BOM above the selection limit");
    const QByteArray exported=readBytes(csv);
    require(exported.contains("BRACKET-50")&&exported.contains(",51,")&&
            exported.count('\n')==2,
            "Full-assembly BOM CSV did not contain the aggregate 51-quantity row");
}

void testAssemblyBomGroupKeyBoundaries() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create BOM-key collision fixture");
    const QString source=doc.parts().first().id;
    require(doc.makeComponent(source,"Shared key fixture",&error)&&
            doc.insertComponentOccurrence(source,20,0,0,&error),
            "Could not create reusable BOM-key instances");
    const QString alternate=doc.parts().last().id;
    // With the previous '|' joined key these two distinct identities both
    // produced: definition|store|sku|p123|widget|a|...
    require(doc.setPartMetadata(source,"P123","Widget","A","Store|SKU",
                                "Manufactured","","",&error)&&
            doc.setPartMetadata(alternate,"SKU|P123","Widget","A","Store",
                                "Manufactured","","",&error),
            "Could not set delimiter-collision metadata");
    const auto distinct=doc.billOfMaterials(&error);
    require(distinct.size()==2&&distinct[0].quantity==1&&distinct[1].quantity==1,
            "BOM combined distinct part metadata because of a delimiter collision");
    require(doc.insertComponentOccurrence(source,40,0,0,&error),
            "Could not insert a genuine matching component for BOM key test");
    const auto grouped=doc.billOfMaterials(&error);
    require(grouped.size()==2,
            "Distinct delimiter-containing identities did not remain separate");
    int total=0,sourceQuantity=0,alternateQuantity=0;
    for(const auto& row:grouped) {
        total+=row.quantity;
        if(row.partNumber=="P123")sourceQuantity=row.quantity;
        if(row.partNumber=="SKU|P123")alternateQuantity=row.quantity;
    }
    require(total==3&&sourceQuantity==2&&alternateQuantity==1,
            "BOM failed to group identical metadata after delimiter-safe encoding");
    require(doc.undo()&&doc.billOfMaterials(&error).size()==2&&
            doc.redo()&&doc.billOfMaterials(&error).size()==2,
            "BOM delimiter-safe grouping was not stable through undo/redo");

    // Check the non-component part-number grouping path independently.
    cad::SceneDocument loose;
    require(loose.addPrimitive(cad::Primitive::Box,{10,10,10},&error)&&
            loose.duplicate(loose.parts().first().id,20,0,0,&error),
            "Could not construct non-component BOM key fixture");
    const QString first=loose.parts().first().id;
    const QString second=loose.parts().last().id;
    require(loose.setPartMetadata(first,"P123","Widget","A","Store|SKU",
                                  "Manufactured","","",&error)&&
            loose.setPartMetadata(second,"SKU|P123","Widget","A","Store",
                                  "Manufactured","","",&error)&&
            loose.billOfMaterials(&error).size()==2,
            "Non-component BOM parts were merged by a delimiter collision");
}

void testOccurrenceBomMetadataInheritance() {
    QString error; cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"BOM inheritance fixture creation failed");
    const QString source=doc.parts().first().id;
    require(doc.makeComponent(source,"Supplier bracket",&error),"BOM inheritance component creation failed");
    require(doc.setPartMetadata(source,"BRK-101","Vendor bracket","C","Machine/Brackets",
            "Purchased","Vendor A","V-101",&error),"BOM inheritance metadata setup failed");
    require(doc.insertComponentOccurrence(source,25,0,0,&error),"BOM inheritance occurrence insertion failed");
    const QString inserted=doc.parts().last().id;
    require(doc.duplicate(source,50,0,0,&error),"BOM inheritance occurrence duplication failed");
    const QString duplicated=doc.parts().last().id;
    for(const QString& id:QStringList{inserted,duplicated}) {
        const cad::Part* part=doc.find(id);
        require(part&&part->partNumber=="BRK-101"&&part->description=="Vendor bracket"&&
                part->revision=="C"&&part->assemblyPath=="Machine/Brackets"&&
                part->procurementType=="Purchased"&&part->supplier=="Vendor A"&&
                part->supplierPartNumber=="V-101","New occurrence lost source component BOM metadata");
    }
    const auto grouped=doc.billOfMaterials(&error);
    require(grouped.size()==1&&grouped.first().quantity==3&&
            grouped.first().partNumber=="BRK-101"&&grouped.first().procurementType=="Purchased",
            "New occurrences did not aggregate under source BOM identity");
    require(doc.undo()&&doc.billOfMaterials(&error).size()==1&&doc.billOfMaterials(&error).first().quantity==2,
            "Duplicated occurrence BOM linkage failed undo");
    require(doc.redo()&&doc.billOfMaterials(&error).size()==1&&doc.billOfMaterials(&error).first().quantity==3,
            "Duplicated occurrence BOM linkage failed redo");
}

void testDrawingSourceRevisionEdits() {
    // A new multi-body sheet with no caller-supplied fingerprint must begin
    // current, independent of source-body ordering. Annotation edits must not
    // acknowledge later geometry changes, including after save/reopen.
    QString revisionError;
    cad::SceneDocument revisionDoc;
    require(revisionDoc.addPrimitive(cad::Primitive::Box,{10,12,14},&revisionError),
            "Implicit drawing-link master creation failed");
    const QString masterId=revisionDoc.parts().first().id;
    require(revisionDoc.addPrimitive(cad::Primitive::Box,{5,6,7},&revisionError),
            "Implicit drawing-link second body creation failed");
    const QString otherId=revisionDoc.parts().last().id;
    cad::DrawingSheetSettings implicitSheet;
    implicitSheet.sourceBodyIds={otherId,masterId};
    require(revisionDoc.setDrawingSheetSettings(masterId,implicitSheet,&revisionError),
            "A new multi-body sheet without an explicit source revision was rejected");
    const auto initialLink=revisionDoc.drawingSheetSourceStatus(masterId);
    require(initialLink.state==cad::DrawingSourceState::Current&&
            initialLink.storedGeometryToken==initialLink.currentGeometryToken,
            "New multi-body drawing was incorrectly marked stale by default");
    QVector<cad::Part> revisionSources={*revisionDoc.find(masterId),*revisionDoc.find(otherId)};
    cad::Part revisionComposite;
    require(cad::buildDrawingAssemblyPart(revisionSources,&revisionComposite,&revisionError)&&
            initialLink.currentGeometryToken==revisionComposite.geometryToken,
            "Implicit drawing revision diverges from the exported assembly fingerprint");
    const QString secondaryGeometryToken=revisionDoc.find(otherId)->geometryToken;
    require(revisionDoc.setMaterial(otherId,"Aluminum 6061",2700,&revisionError)&&
            revisionDoc.find(otherId)->geometryToken==secondaryGeometryToken&&
            revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale&&
            revisionDoc.drawingSheetSourceStatus(masterId).storedDataToken!=revisionDoc.drawingSheetSourceStatus(masterId).currentDataToken,
            "Changing a linked source material did not stale the drawing parts-list revision");
    require(revisionDoc.refreshDrawingSheetSource(masterId,&revisionError)&&
            revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Current&&
            revisionDoc.undo()&&revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale&&
            revisionDoc.redo()&&revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Current,
            "Refreshing linked BOM/material data was not a reversible source-revision update");
    require(revisionDoc.translate(otherId,3,0,0,&revisionError)&&
            revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale,
            "Changing a secondary source body did not stale the saved drawing");
    cad::DrawingSheetSettings noteEdit=revisionDoc.find(masterId)->drawingSettings;
    noteEdit.generalNotes={"Check assembly fit"};
    noteEdit.sourceBodyIds.clear(); // omitted source list retains existing multi-body link
    noteEdit.sourceGeometryToken.clear(); // omitted token is not an implicit refresh
    require(revisionDoc.setDrawingSheetSettings(masterId,noteEdit,&revisionError)&&
            revisionDoc.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale&&
            revisionDoc.find(masterId)->drawingSettings.sourceBodyIds==QStringList({otherId,masterId})&&
            revisionDoc.find(masterId)->drawingSettings.sourceGeometryToken==initialLink.storedGeometryToken,
            "An annotation-only edit lost the source set or cleared the stale revision");
    QTemporaryDir revisionDir;require(revisionDir.isValid(),"Drawing source revision temporary directory failed");
    const QString staleScene=revisionDir.filePath("drawing-source-stale.cadscene");
    require(revisionDoc.saveAs(staleScene,&revisionError),"Could not save a stale linked drawing");
    cad::SceneDocument reopenedRevision;
    require(reopenedRevision.load(staleScene,&revisionError)&&
            reopenedRevision.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale&&
            reopenedRevision.find(masterId)->drawingSettings.sourceDataToken==revisionDoc.find(masterId)->drawingSettings.sourceDataToken&&
            reopenedRevision.find(masterId)->drawingSettings.generalNotes==QStringList{"Check assembly fit"}&&
            reopenedRevision.find(masterId)->drawingSettings.sourceBodyIds==QStringList{otherId,masterId},
            "Save/reopen cleared drawing notes, source identities, or stale source revision");
    require(reopenedRevision.refreshDrawingSheetSource(masterId,&revisionError)&&
            reopenedRevision.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Current,
            "Explicit drawing-source refresh did not resolve stale multi-body geometry");
    QJsonObject legacyDataScene=QJsonDocument::fromJson(readBytes(staleScene)).object();
    QJsonArray legacyDataParts=legacyDataScene.value("parts").toArray();
    for(int i=0;i<legacyDataParts.size();++i){
        QJsonObject record=legacyDataParts[i].toObject();
        QJsonObject drawing=record.value("drawing_settings").toObject();
        if(!drawing.isEmpty()){drawing.remove("source_data_token");record.insert("drawing_settings",drawing);legacyDataParts[i]=record;}
    }
    legacyDataScene.insert("parts",legacyDataParts);
    const QString legacyDataPath=revisionDir.filePath("drawing-source-legacy-data.cadscene");
    writeBytes(legacyDataPath,QJsonDocument(legacyDataScene).toJson());
    cad::SceneDocument migratedDataScene;
    require(migratedDataScene.load(legacyDataPath,&revisionError)&&
            migratedDataScene.drawingSheetSourceStatus(masterId).state==cad::DrawingSourceState::Stale&&
            !migratedDataScene.find(masterId)->drawingSettings.sourceDataToken.isEmpty(),
            "A pre-metadata-revision drawing did not migrate its source-data baseline safely");
    // A partial update may omit stable identities, but it is not a new sheet.
    cad::DrawingSheetSettings originalSheet=reopenedRevision.find(masterId)->drawingSettings;
    originalSheet.generalNotes={"Inspect assembly fit"};
    originalSheet.noteAnnotations[0].xMm=24.0;
    originalSheet.noteAnnotations[0].yMm=28.0;
    require(reopenedRevision.setDrawingSheetSettings(masterId,originalSheet,&revisionError),
            "Could not seed drawing identity-preservation regression");
    const cad::DrawingSheetSettings beforePartial=reopenedRevision.find(masterId)->drawingSettings;
    cad::DrawingSheetSettings partial;
    partial.generalNotes={"Inspect fit again"};
    require(reopenedRevision.setDrawingSheetSettings(masterId,partial,&revisionError),
            "Partial drawing-sheet edit unexpectedly failed");
    const cad::DrawingSheetSettings afterPartial=reopenedRevision.find(masterId)->drawingSettings;
    require(afterPartial.sheetId==beforePartial.sheetId&&
            afterPartial.viewIds==beforePartial.viewIds&&
            afterPartial.partsListAnnotation.id==beforePartial.partsListAnnotation.id&&
            afterPartial.dimensionAnnotations.size()==beforePartial.dimensionAnnotations.size()&&
            afterPartial.noteAnnotations.size()==1&&
            afterPartial.noteAnnotations[0].id==beforePartial.noteAnnotations[0].id&&
            afterPartial.noteAnnotations[0].xMm==beforePartial.noteAnnotations[0].xMm&&
            afterPartial.noteAnnotations[0].yMm==beforePartial.noteAnnotations[0].yMm&&
            afterPartial.noteAnnotations[0].text=="Inspect fit again",
            "Partial drawing edit regenerated persistent sheet/view/annotation identities");
    for(int i=0;i<afterPartial.dimensionAnnotations.size();++i)
        require(afterPartial.dimensionAnnotations[i].id==beforePartial.dimensionAnnotations[i].id,
                "Partial drawing edit regenerated overall dimension identities");
    require(reopenedRevision.saveAs(staleScene,&revisionError),"Could not save partial drawing identity edit");
    cad::SceneDocument reopenedPartial;
    require(reopenedPartial.load(staleScene,&revisionError),"Could not reload partial drawing identity edit");
    require(reopenedPartial.find(masterId)&&
            reopenedPartial.find(masterId)->drawingSettings.sheetId==beforePartial.sheetId&&
            reopenedPartial.find(masterId)->drawingSettings.viewIds==beforePartial.viewIds&&
            reopenedPartial.find(masterId)->drawingSettings.noteAnnotations[0].id==beforePartial.noteAnnotations[0].id,
            "Save/reopen lost stable drawing identities after partial edit");
    // Saving an unchanged sheet must not introduce another undo step.
    cad::SceneDocument noOpDrawing;
    require(noOpDrawing.addPrimitive(cad::Primitive::Box,{6,7,8},&revisionError),
            "No-op sheet history fixture failed");
    const QString noOpId=noOpDrawing.parts().first().id;
    require(noOpDrawing.setDrawingSheetSettings(noOpId,cad::DrawingSheetSettings{},&revisionError),
            "No-op sheet initial save failed");
    const cad::DrawingSheetSettings unchanged=noOpDrawing.find(noOpId)->drawingSettings;
    require(noOpDrawing.setDrawingSheetSettings(noOpId,unchanged,&revisionError),
            "No-op sheet resave was rejected");
    require(noOpDrawing.undo()&&!noOpDrawing.find(noOpId)->hasDrawingSettings,
            "Saving unchanged sheet created an extra undo record");
    require(noOpDrawing.redo()&&noOpDrawing.find(noOpId)->drawingSettings==unchanged,
            "Redo did not restore saved drawing after a no-op edit");
    cad::SceneDocument externalSource;
    require(externalSource.addPrimitive(cad::Primitive::Box,{8,9,10},&revisionError)&&
            externalSource.addPrimitive(cad::Primitive::Box,{4,5,6},&revisionError),
            "External single-body source fixture creation failed");
    cad::DrawingSheetSettings externalSheet;
    externalSheet.sourceBodyIds={externalSource.parts().last().id};
    const QString externalMaster=externalSource.parts().first().id;
    require(externalSource.setDrawingSheetSettings(externalMaster,externalSheet,&revisionError)&&
            externalSource.drawingSheetSourceStatus(externalMaster).state==cad::DrawingSourceState::Current,
            "New single-source drawing used its sheet owner's unrelated geometry token");
}

void testDrawingExportPreflightDoesNotAcknowledgeFailedSnapshot() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{18,12,8},&error),
            "Drawing export preflight source creation failed");
    const QString id=doc.parts().first().id;
    cad::DrawingSheetSettings sheet;
    sheet.generalNotes={"Verify fits after model changes"};
    require(doc.setDrawingSheetSettings(id,sheet,&error),"Could not save export-preflight fixture");
    const auto before=doc.find(id)->drawingSettings;
    require(doc.translate(id,2,0,0,&error)&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
            "Drawing source did not become stale after geometry changed");
    cad::DrawingSheetSettings update=before;
    update.sourceGeometryToken=doc.find(id)->geometryToken;
    update.generalNotes={"Inspect revised part"};
    cad::DrawingSheetSettings prepared;
    require(!doc.prepareDrawingSheetSettings(id,update,nullptr,&error),
            "Drawing preflight accepted a missing output record");
    require(doc.prepareDrawingSheetSettings(id,update,&prepared,&error)&&
            prepared.sheetId==before.sheetId&&prepared.viewIds==before.viewIds&&
            prepared.noteAnnotations.first().id==before.noteAnnotations.first().id&&
            prepared.sourceGeometryToken==doc.find(id)->geometryToken&&
            doc.find(id)->drawingSettings==before&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
            "Export preflight changed the saved sheet or lost placed annotation identities");
    QTemporaryDir dir;require(dir.isValid(),"Drawing export preflight temp directory missing");
    const QString missingDirectory=dir.filePath("missing/snapshot.svg");
    require(!cad::exportOrthographicDrawingSvg(*doc.find(id),missingDirectory,&error),
            "Drawing snapshot unexpectedly succeeded in a missing directory");
    require(doc.find(id)->drawingSettings==before&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
            "Failed snapshot acknowledged geometry or changed the saved annotations");
    // Failed preflight/export must leave undo pointed at the geometry operation.
    require(doc.undo()&&doc.find(id)->hasDrawingSettings&&
            doc.find(id)->drawingSettings==before&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Current&&
            doc.redo()&&doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
            "Failed snapshot consumed unrelated geometry history");
    QByteArray snapshot;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&snapshot,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
            cad::DrawingAuxiliaryView::Isometric,prepared.sectionDepthFraction,prepared.generalNotes,{},
            prepared.viewCenters,prepared.viewIds,prepared.sheetId,prepared.noteAnnotations,
            prepared.dimensionAnnotations),
            "Could not render the prepared sheet without saving it");
    require(snapshot.contains(prepared.sheetId.toUtf8())&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
            "Rendering a snapshot changed the saved drawing source revision");
    require(doc.setDrawingSheetSettings(id,prepared,&error)&&
            doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Current&&
            doc.find(id)->drawingSettings.noteAnnotations.first().id==before.noteAnnotations.first().id,
            "Successful snapshot acknowledgement changed placed annotation identity");
    require(doc.undo()&&doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale&&
            doc.redo()&&doc.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Current,
            "Drawing export acknowledgement is not a single undoable edit");
}

void testDrawingSheetIdentityUniqueness() {
    QString error;cad::SceneDocument scene;
    require(scene.addPrimitive(cad::Primitive::Box,{9,8,7},&error)&&
            scene.addPrimitive(cad::Primitive::Box,{4,5,6},&error),
            "Two-sheet uniqueness source creation failed");
    const QString first=scene.parts().first().id,second=scene.parts().last().id;
    require(scene.setDrawingSheetSettings(first,cad::DrawingSheetSettings{},&error)&&
            scene.setDrawingSheetSettings(second,cad::DrawingSheetSettings{},&error),
            "Could not set up two independent persistent drawing sheets");
    const QString sheetId=scene.find(first)->drawingSettings.sheetId;
    require(scene.find(second)->drawingSettings.sheetId!=sheetId,
            "Distinct drawing sheets received identical default identities");
    cad::DrawingSheetSettings collision=scene.find(second)->drawingSettings;
    collision.sheetId=sheetId;
    require(!scene.setDrawingSheetSettings(second,collision,&error)&&
            scene.find(second)->drawingSettings.sheetId!=sheetId,
            "Drawing editor accepted another sheet's persistent identity");
    QTemporaryDir dir;require(dir.isValid(),"Drawing identity fixture temp directory missing");
    const QString validPath=dir.filePath("two-sheets.cadscene");
    require(scene.saveAs(validPath,&error),"Could not serialize two valid drawing sheets");
    QJsonObject root=QJsonDocument::fromJson(readBytes(validPath)).object();
    QJsonArray parts=root.value("parts").toArray();
    require(parts.size()==2,"Two-sheet project JSON did not contain exactly two parts");
    QJsonObject secondPart=parts[1].toObject();
    QJsonObject secondDrawing=secondPart.value("drawing_settings").toObject();
    require(!secondDrawing.isEmpty(),"Second sheet JSON record missing");
    secondDrawing.insert("sheet_id",sheetId);
    secondPart.insert("drawing_settings",secondDrawing);parts[1]=secondPart;
    root.insert("parts",parts);
    const QString corruptedPath=dir.filePath("duplicate-sheet-id.cadscene");
    writeBytes(corruptedPath,QJsonDocument(root).toJson());
    require(!scene.load(corruptedPath,&error)&&scene.parts().size()==2&&
            scene.find(first)->drawingSettings.sheetId==sheetId&&
            scene.find(second)->drawingSettings.sheetId!=sheetId,
            "Invalid duplicate sheet IDs were loaded or mutated the open scene");
    cad::SceneDocument reopened;
    require(reopened.load(validPath,&error)&&reopened.find(first)&&reopened.find(second)&&
            reopened.find(first)->drawingSettings.sheetId!=reopened.find(second)->drawingSettings.sheetId,
            "Two distinct saved drawing identities did not survive reopening");
    // XML attribute whitespace normalization must not mutate persistent IDs.
    cad::DrawingSheetSettings whitespaceId=scene.find(first)->drawingSettings;
    whitespaceId.viewIds.insert("FRONT",QStringLiteral("view\tfront"));
    require(!scene.setDrawingSheetSettings(first,whitespaceId,&error)&&
            scene.find(first)->drawingSettings.viewIds.value("FRONT")!="view\tfront",
            "XML-normalized tab was accepted in a persistent view ID");
    whitespaceId=scene.find(first)->drawingSettings;
    whitespaceId.sheetId=QStringLiteral("sheet\nnext");
    require(!scene.setDrawingSheetSettings(first,whitespaceId,&error),
            "XML-normalized newline was accepted in a persistent sheet ID");
    QJsonObject whitespaceRoot=QJsonDocument::fromJson(readBytes(validPath)).object();
    QJsonArray whitespaceParts=whitespaceRoot.value("parts").toArray();
    QJsonObject firstPart=whitespaceParts[0].toObject();
    QJsonObject firstDrawing=firstPart.value("drawing_settings").toObject();
    QJsonObject views=firstDrawing.value("view_ids").toObject();
    views.insert("FRONT",QStringLiteral("tampered\tview"));
    firstDrawing.insert("view_ids",views);firstPart.insert("drawing_settings",firstDrawing);
    whitespaceParts[0]=firstPart;whitespaceRoot.insert("parts",whitespaceParts);
    const QString whitespacePath=dir.filePath("invalid-xml-id-whitespace.cadscene");
    writeBytes(whitespacePath,QJsonDocument(whitespaceRoot).toJson());
    require(!scene.load(whitespacePath,&error)&&scene.parts().size()==2&&
            scene.find(first)->drawingSettings.viewIds.value("FRONT")!="tampered\tview",
            "Loader accepted a drawing view identity that XML attribute parsing would normalize");
    // A comma in the source ID cannot be encoded unambiguously in SVG metadata.
    QJsonObject commaRoot=QJsonDocument::fromJson(readBytes(validPath)).object();
    QJsonArray commaParts=commaRoot.value("parts").toArray();
    QJsonObject commaPart=commaParts[0].toObject();
    QJsonObject commaDrawing=commaPart.value("drawing_settings").toObject();
    const QString commaId=QStringLiteral("body,one");
    commaPart.insert("id",commaId);
    QJsonArray commaBodies;commaBodies.append(commaId);
    commaDrawing.insert("source_body_ids",commaBodies);
    commaPart.insert("drawing_settings",commaDrawing);
    commaParts[0]=commaPart;commaRoot.insert("parts",commaParts);
    const QString commaPath=dir.filePath("invalid-comma-body-id.cadscene");
    writeBytes(commaPath,QJsonDocument(commaRoot).toJson());
    require(!scene.load(commaPath,&error)&&scene.parts().size()==2,
            "Loader accepted an ambiguous comma-separated drawing source ID");
}

void testPersistentAssemblyOccurrences() {
    QString statusError;cad::SceneDocument singleSourceStatus;
    require(singleSourceStatus.addPrimitive(cad::Primitive::Box,{5,6,7},&statusError),"Single-source status fixture body failed");
    const QString singleSourceId=singleSourceStatus.parts().first().id;
    require(singleSourceStatus.drawingSheetSourceStatus(singleSourceId).state==cad::DrawingSourceState::NoSavedSheet,
            "An unsaved drawing sheet did not report the no-sheet state");
    require(singleSourceStatus.setDrawingSheetSettings(singleSourceId,cad::DrawingSheetSettings{},&statusError)&&
            singleSourceStatus.drawingSheetSourceStatus(singleSourceId).state==cad::DrawingSourceState::Current,
            "A legacy single-body sheet did not recognize its own geometry revision");
    require(singleSourceStatus.translate(singleSourceId,1,0,0,&statusError)&&
            singleSourceStatus.drawingSheetSourceStatus(singleSourceId).state==cad::DrawingSourceState::Stale,
            "A legacy single-body sheet did not report changed geometry as stale");

    QString error; cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Assembly component source creation failed");
    const QString source=doc.parts().first().id;
    require(!doc.insertComponentOccurrence(source,50,0,0,&error),"Non-component body was accepted as a reusable occurrence source");
    require(!doc.makeComponent(source,"\n",&error),"Invalid component name was accepted");
    require(doc.makeComponent(source,"Mounting bracket",&error),"Could not convert body into a component definition");
    cad::DrawingSheetSettings drawing;drawing.projection="First-angle";drawing.sheetSize="A3";drawing.auxiliaryView="Section A-A";
    drawing.sectionDepthFraction=0.35;drawing.generalNotes={"Deburr edges","Inspect bore"};drawing.drawingNumber="ASM-DRW-01";
    drawing.drawnBy="QA";drawing.checkedBy="Reviewer";drawing.issueDate="2026-10-27";drawing.revision="B";
    drawing.viewCenters.insert("FRONT",QPointF(120,140));
    drawing.noteAnnotations={{"note-deburr","Deburr edges",24,240},{"note-bore","Inspect bore",24,246}};
    drawing.dimensionAnnotations={{"dim-x","X",12.5},{"dim-y","Y",15.0},{"dim-z","Z",18.0}};
    drawing.partsListAnnotation={"bom-table-asm",true,25,20};
    drawing.balloonAnnotations={{"balloon-front",source,250,170}};
    drawing.detailAnnotations={{"detail-front","FRONT",0,0,5,350,120,20}};
    drawing.sectionAnnotations={{"section-b","Y",0.35,320,180}};
    require(doc.setDrawingSheetSettings(source,drawing,&error),"Valid persistent drawing setup was rejected");
    require(doc.undo()&&!doc.find(source)->hasDrawingSettings,"Drawing setup edit did not undo as one history step");
    require(doc.redo()&&doc.find(source)->hasDrawingSettings,"Drawing setup edit did not redo");
    // Invalid XML control characters must never reach a saved drawing or SVG export.
    const QString forbiddenXml=QString(QChar(0x0001));
    cad::DrawingSheetSettings invalidBalloonId=drawing;
    invalidBalloonId.balloonAnnotations[0].id="balloon"+forbiddenXml;
    require(!doc.setDrawingSheetSettings(source,invalidBalloonId,&error),
            "XML-forbidden control in a persisted balloon identity was accepted");
    cad::DrawingSheetSettings invalidDetailId=drawing;
    invalidDetailId.detailAnnotations[0].id="detail"+forbiddenXml;
    require(!doc.setDrawingSheetSettings(source,invalidDetailId,&error),
            "XML-forbidden control in a detail-view identity was accepted");
    cad::DrawingSheetSettings invalidTitle=drawing;
    invalidTitle.drawingNumber="DWG"+forbiddenXml;
    require(!doc.setDrawingSheetSettings(source,invalidTitle,&error),
            "XML-forbidden control in drawing title-block text was accepted");
    require(doc.find(source)->drawingSettings.balloonAnnotations.first().id=="balloon-front"&&
            doc.find(source)->drawingSettings.drawingNumber=="ASM-DRW-01",
            "Rejected drawing XML text modified the previously saved sheet");
    // UTF-16 surrogates must be paired; valid non-BMP Unicode remains legal.
    cad::DrawingSheetSettings unpairedHigh=drawing;
    unpairedHigh.noteAnnotations[0].text=QStringLiteral("note ")+QString(QChar(0xD83D));
    unpairedHigh.generalNotes[0]=unpairedHigh.noteAnnotations[0].text;
    require(!doc.setDrawingSheetSettings(source,unpairedHigh,&error),
            "Unpaired high surrogate in drawing note was accepted");
    cad::DrawingSheetSettings unpairedLow=drawing;
    unpairedLow.balloonAnnotations[0].id=QStringLiteral("balloon-")+QString(QChar(0xDE00));
    require(!doc.setDrawingSheetSettings(source,unpairedLow,&error),
            "Unpaired low surrogate in drawing balloon ID was accepted");
    cad::DrawingSheetSettings brokenPair=drawing;
    brokenPair.noteAnnotations[0].text=QString(QChar(0xD83D))+QStringLiteral("X")+QString(QChar(0xDE00));
    brokenPair.generalNotes[0]=brokenPair.noteAnnotations[0].text;
    require(!doc.setDrawingSheetSettings(source,brokenPair,&error),
            "Nonadjacent UTF-16 surrogate units in drawing note were accepted");
    require(doc.find(source)->drawingSettings.noteAnnotations[0].text=="Deburr edges"&&
            doc.find(source)->drawingSettings.balloonAnnotations[0].id=="balloon-front",
            "Rejected surrogate text modified the existing drawing");
    cad::DrawingSheetSettings pairedUnicode=drawing;
    pairedUnicode.noteAnnotations[0].text=QStringLiteral("Inspect ")+QString(QChar(0xD83D))+QString(QChar(0xDD0D));
    pairedUnicode.generalNotes[0]=pairedUnicode.noteAnnotations[0].text;
    require(doc.setDrawingSheetSettings(source,pairedUnicode,&error),
            "Valid supplementary Unicode character was rejected");
    require(doc.find(source)->drawingSettings.noteAnnotations[0].text==pairedUnicode.noteAnnotations[0].text,
            "Valid supplementary Unicode drawing note was not retained");
    require(doc.setDrawingSheetSettings(source,drawing,&error),
            "Could not restore drawing after supplementary Unicode regression");
    cad::DrawingSheetSettings invalidDrawing=drawing;invalidDrawing.issueDate="27/10/2026";
    require(!doc.setDrawingSheetSettings(source,invalidDrawing,&error),"Invalid drawing issue date was accepted in persistent settings");
    cad::DrawingSheetSettings outsideView=drawing;outsideView.viewCenters.insert("FRONT",QPointF(5,5));
    require(!doc.setDrawingSheetSettings(source,outsideView,&error),"Out-of-sheet view placement was accepted");
    cad::DrawingSheetSettings duplicateAnnotation=drawing;duplicateAnnotation.noteAnnotations[1].id="note-deburr";
    require(!doc.setDrawingSheetSettings(source,duplicateAnnotation,&error),"Duplicate persistent note identity was accepted");
    cad::DrawingSheetSettings invalidDimension=drawing;invalidDimension.dimensionAnnotations[2].offsetMm=61;
    require(!doc.setDrawingSheetSettings(source,invalidDimension,&error),"Out-of-range persistent dimension placement was accepted");
    cad::DrawingSheetSettings invalidPartsList=drawing;invalidPartsList.partsListAnnotation.xMm=280;
    require(!doc.setDrawingSheetSettings(source,invalidPartsList,&error),"Out-of-sheet linked parts-list placement was accepted");
    cad::DrawingSheetSettings missingBalloonSource=drawing;missingBalloonSource.balloonAnnotations[0].sourceBodyId="missing-source";
    require(!doc.setDrawingSheetSettings(source,missingBalloonSource,&error),"Drawing balloon accepted a body outside the sheet source set");
    cad::DrawingSheetSettings outsideBalloon=drawing;outsideBalloon.balloonAnnotations[0].xMm=2;
    require(!doc.setDrawingSheetSettings(source,outsideBalloon,&error),"Out-of-sheet BOM balloon position was accepted");
    cad::DrawingSheetSettings outsideDetail=drawing;outsideDetail.detailAnnotations[0].xMm=20;
    require(!doc.setDrawingSheetSettings(source,outsideDetail,&error),"Out-of-sheet detail-view placement was accepted");
    cad::DrawingSheetSettings outsideSection=drawing;outsideSection.sectionAnnotations[0].xMm=10;
    require(!doc.setDrawingSheetSettings(source,outsideSection,&error),"Out-of-sheet independent section placement was accepted");
    cad::DrawingSheetSettings invalidSectionAxis=drawing;invalidSectionAxis.sectionAnnotations[0].planeAxis="diagonal";
    require(!doc.setDrawingSheetSettings(source,invalidSectionAxis,&error),"Unsupported independent section plane axis was accepted");
    const QString definition=doc.find(source)->componentDefinitionId;
    const QString firstOccurrence=doc.find(source)->occurrenceId;
    require(!definition.isEmpty()&&!firstOccurrence.isEmpty()&&doc.find(source)->componentName=="Mounting bracket",
            "Component definition or first occurrence identity was not assigned");
    require(doc.insertComponentOccurrence(source,50,-10,5,&error),"Fixed component occurrence placement failed");
    const cad::Part placed=doc.parts().last();
    require(placed.componentDefinitionId==definition&&placed.componentName=="Mounting bracket"&&placed.occurrenceId!=firstOccurrence,
            "Inserted occurrence did not reference the reusable definition with its own identity");
    closeTo(placed.placementX,50);closeTo(placed.placementY,-10);closeTo(placed.placementZ,5);
    cad::BodyDiagnostics placedGeometry;require(doc.diagnoseBody(placed.id,&placedGeometry,&error),"Placed component geometry could not be inspected");
    require(placedGeometry.minX==50&&placedGeometry.maxX==60&&placedGeometry.minY==-10&&placedGeometry.maxY==10&&
            placedGeometry.minZ==5&&placedGeometry.maxZ==35,"Fixed translation offsets did not move occurrence geometry by the requested XYZ values");
    require(doc.parts().size()==2,"Assembly occurrence was not inserted into the scene");
    const auto componentParts=doc.partsList(&error);
    require(componentParts.size()==2&&componentParts[0].componentDefinitionId==definition&&componentParts[0].componentName=="Mounting bracket"&&
            !componentParts[0].occurrenceId.isEmpty()&&componentParts[1].componentDefinitionId==definition,
            "Per-occurrence parts list omitted component or occurrence identity");
    closeTo(componentParts[1].placementX,50);closeTo(componentParts[1].placementY,-10);closeTo(componentParts[1].placementZ,5);
    QTemporaryDir dir;require(dir.isValid(),"Assembly persistence temporary directory failed");
    const QString componentBomPath=dir.filePath("component-bom.csv"),componentListPath=dir.filePath("component-list.csv");
    require(doc.exportBillOfMaterialsCsv(componentBomPath,&error)&&doc.exportPartsListCsv(componentListPath,&error),
            "Component-linked BOM or per-occurrence parts-list CSV export failed");
    const QByteArray componentBomCsv=readBytes(componentBomPath),componentListCsv=readBytes(componentListPath);
    require(componentBomCsv.contains(definition.toUtf8())&&componentBomCsv.contains("Mounting bracket")&&
            componentListCsv.contains(definition.toUtf8())&&componentListCsv.contains(firstOccurrence.toUtf8())&&
            componentListCsv.contains("Translation offset X (mm)")&&componentListCsv.contains(",50,-10,5"),
            "Assembly identities or fixed placements were omitted from CSV reports");
    const auto bom=doc.billOfMaterials(&error);
    require(bom.size()==1&&bom.first().quantity==2&&bom.first().componentDefinitionId==definition&&bom.first().componentName=="Mounting bracket",
            "Reusable occurrences were not aggregated and linked in the BOM");
    const auto occurrenceScopedBom=doc.billOfMaterials(QStringList{source,placed.id},&error);
    require(occurrenceScopedBom.size()==1&&occurrenceScopedBom.first().quantity==2&&occurrenceScopedBom.first().componentDefinitionId==definition,
            "Source-scoped drawing BOM failed to group the selected reusable occurrences");
    require(doc.scale(placed.id,2.0,&error),"Occurrence geometry edit fixture could not be scaled");
    require(doc.billOfMaterials(&error).size()==2,"Geometrically divergent occurrences were incorrectly merged into one BOM line");
    require(doc.undo()&&doc.billOfMaterials(&error).size()==1,"Occurrence geometry edit undo did not restore BOM aggregation");
    require(doc.undo()&&doc.parts().size()==1,"Assembly occurrence insertion did not undo as one operation");
    require(doc.redo()&&doc.parts().size()==2,"Assembly occurrence insertion did not redo");
    drawing.sourceBodyIds={source,placed.id};cad::Part drawingSourceComposite;
    require(cad::buildDrawingAssemblyPart(QVector<cad::Part>{*doc.find(source),*doc.find(placed.id)},&drawingSourceComposite,&error),
            "Could not build the source revision for the persistent drawing status fixture");
    drawing.sourceGeometryToken=drawingSourceComposite.geometryToken;
    require(doc.setDrawingSheetSettings(source,drawing,&error),"A two-body linked drawing source could not be persisted");
    require(doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Current,
            "Saved drawing source status did not recognize the current two-body geometry revision");
    require(doc.translate(placed.id,3,0,0,&error)&&doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Stale,
            "Saved drawing source status did not detect changed source geometry");
    require(doc.refreshDrawingSheetSource(source,&error)&&doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Current,
            "Saved drawing link could not be refreshed to the current multi-body source revision");
    require(doc.undo()&&doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Stale&&doc.redo()&&
            doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Current,
            "Drawing link refresh did not preserve the stale/current state through undo and redo");
    require(doc.undo()&&doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Stale&&
            doc.undo()&&doc.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::Current,
            "Saved drawing source status did not return to current after undoing both refresh and geometry edit");
    require(!doc.refreshDrawingSheetSource("missing-master",&error),"A missing drawing sheet master was refreshed");
    cad::DrawingSheetSettings missingSource=drawing;missingSource.sourceBodyIds={source,"missing-source"};
    require(!doc.setDrawingSheetSettings(source,missingSource,&error),"A missing drawing source body was accepted");

    const QString scene=dir.filePath("assembly.cadscene");
    require(doc.saveAs(scene,&error),"Assembly scene save failed");
    QJsonObject root=QJsonDocument::fromJson(readBytes(scene)).object();
    require(root.value("schema").toString()=="cadworkbench.scene.v22","New project did not use scene schema v22");
    QJsonObject schema13Root=root;schema13Root.insert("schema","cadworkbench.scene.v13");
    QJsonArray schema13Parts=schema13Root.value("parts").toArray();
    for(int i=0;i<schema13Parts.size();++i){
        QJsonObject record=schema13Parts[i].toObject();
        if(record.value("drawing_settings").isObject()){
            QJsonObject settings=record.value("drawing_settings").toObject();
            settings.remove("detail_views");settings.remove("section_views");record.insert("drawing_settings",settings);
        }
        schema13Parts[i]=record;
    }
    schema13Root.insert("parts",schema13Parts);
    const QString schema13Scene=dir.filePath("assembly-v13.cadscene");writeBytes(schema13Scene,QJsonDocument(schema13Root).toJson());
    cad::SceneDocument migrated13;error.clear();const bool migrated13Ok=migrated13.load(schema13Scene,&error);
    require(migrated13Ok,("Schema-v13 drawing project did not migrate to v14 defaults: "+error.toStdString()).c_str());
    require(migrated13.find(source)->drawingSettings.detailAnnotations.isEmpty()&&
            migrated13.find(source)->drawingSettings.sectionAnnotations.isEmpty()&&
            migrated13.find(source)->drawingSettings.balloonAnnotations.size()==1&&
            migrated13.drawingReadinessPolicy().requiredTitleBlockFields==doc.drawingReadinessPolicy().requiredTitleBlockFields,
            "Schema-v13 migration lost existing drawing balloons/policy or failed to default detail views");
    QJsonObject schema14Root=root;schema14Root.insert("schema","cadworkbench.scene.v14");
    QJsonArray schema14Parts=schema14Root.value("parts").toArray();
    for(int i=0;i<schema14Parts.size();++i){
        QJsonObject record=schema14Parts[i].toObject();
        if(record.value("drawing_settings").isObject()){
            QJsonObject settings=record.value("drawing_settings").toObject();settings.remove("section_views");
            record.insert("drawing_settings",settings);
        }
        schema14Parts[i]=record;
    }
    schema14Root.insert("parts",schema14Parts);
    const QString schema14Scene=dir.filePath("assembly-v14.cadscene");writeBytes(schema14Scene,QJsonDocument(schema14Root).toJson());
    cad::SceneDocument migrated14;error.clear();const bool migrated14Ok=migrated14.load(schema14Scene,&error);
    require(migrated14Ok,("Schema-v14 detail drawing did not migrate to v15: "+error.toStdString()).c_str());
    require(migrated14.find(source)->drawingSettings.detailAnnotations.size()==1&&
            migrated14.find(source)->drawingSettings.sectionAnnotations.isEmpty(),
            "Schema-v14 migration lost saved detail views or failed to default separate sections");
    cad::SceneDocument loaded;require(loaded.load(scene,&error),("Assembly scene reload failed: "+error.toStdString()).c_str());
    require(loaded.parts().size()==2&&loaded.find(source)->componentDefinitionId==definition&&loaded.parts().last().occurrenceId==placed.occurrenceId,
            "Component/occurrence identities did not persist across reload");
    closeTo(loaded.parts().last().placementY,-10);
    const auto loadedBom=loaded.billOfMaterials(&error);
    require(loadedBom.size()==1&&loadedBom.first().quantity==2&&loadedBom.first().componentName=="Mounting bracket",
            "BOM component link or grouped count changed after reload");
    const auto persistedParts=loaded.partsList(&error);
    require(persistedParts.size()==2&&persistedParts[1].componentDefinitionId==definition&&persistedParts[1].placementX==50,
            "Per-occurrence component identity or placement was lost after scene reload");
    require(loaded.find(source)->hasDrawingSettings&&loaded.find(source)->drawingSettings.projection=="First-angle"&&
            loaded.find(source)->drawingSettings.sheetSize=="A3"&&loaded.find(source)->drawingSettings.auxiliaryView=="Section A-A"&&
            loaded.find(source)->drawingSettings.generalNotes==QStringList({"Deburr edges","Inspect bore"})&&
            loaded.find(source)->drawingSettings.drawingNumber=="ASM-DRW-01"&&loaded.find(source)->drawingSettings.drawnBy=="QA"&&
            loaded.find(source)->drawingSettings.checkedBy=="Reviewer"&&loaded.find(source)->drawingSettings.issueDate=="2026-10-27"&&
            loaded.find(source)->drawingSettings.revision=="B"&&loaded.find(source)->drawingSettings.detailAnnotations.size()==1&&
            loaded.find(source)->drawingSettings.detailAnnotations.first().id=="detail-front"&&
            loaded.find(source)->drawingSettings.detailAnnotations.first().sourceView=="FRONT"&&
            loaded.find(source)->drawingSettings.detailAnnotations.first().radiusMm==20&&
            loaded.find(source)->drawingSettings.sectionAnnotations.size()==1&&
            loaded.find(source)->drawingSettings.sectionAnnotations.first().id=="section-b"&&
            loaded.find(source)->drawingSettings.sectionAnnotations.first().planeAxis=="Y"&&
            std::abs(loaded.find(source)->drawingSettings.sectionAnnotations.first().depthFraction-0.35)<1e-9,
            "Persistent drawing setup or annotations did not survive project reload");
    require(loaded.eraseMany(QStringList{placed.id},&error)&&
            loaded.drawingSheetSourceStatus(source).state==cad::DrawingSourceState::MissingBodies&&
            loaded.drawingSheetSourceStatus(source).missingSourceBodyIds==QStringList{placed.id},
            "Saved drawing source status did not identify a deleted source body");
    require(!loaded.find(source)->drawingSettings.sheetId.isEmpty()&&
            loaded.find(source)->drawingSettings.sheetId==doc.find(source)->drawingSettings.sheetId&&
            loaded.find(source)->drawingSettings.viewIds==doc.find(source)->drawingSettings.viewIds&&
            loaded.find(source)->drawingSettings.noteAnnotations.size()==2&&
            loaded.find(source)->drawingSettings.noteAnnotations[0].id=="note-deburr"&&
            loaded.find(source)->drawingSettings.dimensionAnnotations.size()==3&&
            loaded.find(source)->drawingSettings.dimensionAnnotations[0].id=="dim-x"&&
            loaded.find(source)->drawingSettings.partsListAnnotation.id=="bom-table-asm"&&
            loaded.find(source)->drawingSettings.partsListAnnotation.visible&&
            loaded.find(source)->drawingSettings.balloonAnnotations.size()==1&&
            loaded.find(source)->drawingSettings.balloonAnnotations.first().id=="balloon-front"&&
            loaded.find(source)->drawingSettings.balloonAnnotations.first().sourceBodyId==source&&
            loaded.find(source)->drawingSettings.sourceBodyIds==QStringList({source,placed.id})&&
            loaded.find(source)->drawingSettings.sourceGeometryToken==drawing.sourceGeometryToken&&
            loaded.find(source)->drawingSettings.viewCenters.value("FRONT")==QPointF(120,140),
            "Persistent sheet, placed-view, or note identities/coordinates changed after reload");
    closeTo(loaded.find(source)->drawingSettings.noteAnnotations[1].xMm,24);
    closeTo(loaded.find(source)->drawingSettings.dimensionAnnotations[1].offsetMm,15.0);
    closeTo(loaded.find(source)->drawingSettings.sectionDepthFraction,0.35);

    QJsonObject schema11Root=root;schema11Root.insert("schema","cadworkbench.scene.v11");
    QJsonArray schema11Records=schema11Root.value("parts").toArray();
    for(int i=0;i<schema11Records.size();++i){QJsonObject item=schema11Records[i].toObject();QJsonObject settings=item.value("drawing_settings").toObject();if(!settings.isEmpty()){settings.remove("balloon_annotations");item.insert("drawing_settings",settings);}schema11Records[i]=item;}
    schema11Root.insert("parts",schema11Records);
    const QString schema11Path=dir.filePath("assembly-v11.cadscene");QFile schema11File(schema11Path);
    require(schema11File.open(QIODevice::WriteOnly),"Could not create schema-v11 drawing migration fixture");
    require(schema11File.write(QJsonDocument(schema11Root).toJson())>0,"Could not write schema-v11 drawing fixture");schema11File.close();
    cad::SceneDocument migrated11;require(migrated11.load(schema11Path,&error),("Schema-v11 scene migration failed: "+error.toStdString()).c_str());
    require(migrated11.find(source)->drawingSettings.balloonAnnotations.isEmpty()&&
            migrated11.find(source)->drawingSettings.partsListAnnotation.id=="bom-table-asm",
            "Schema-v11 drawing migration did not preserve the parts list while defaulting balloons to empty");

    QJsonObject schema9Root=root;schema9Root.insert("schema","cadworkbench.scene.v9");
    QJsonArray schema9Records=schema9Root.value("parts").toArray();
    for(int i=0;i<schema9Records.size();++i){QJsonObject item=schema9Records[i].toObject();QJsonObject settings=item.value("drawing_settings").toObject();if(!settings.isEmpty()){settings.remove("dimension_annotations");settings.remove("linked_parts_list");item.insert("drawing_settings",settings);}schema9Records[i]=item;}
    schema9Root.insert("parts",schema9Records);
    const QString schema9Path=dir.filePath("assembly-v9.cadscene");QFile schema9File(schema9Path);
    require(schema9File.open(QIODevice::WriteOnly),"Could not create schema-v9 dimension migration fixture");
    schema9File.write(QJsonDocument(schema9Root).toJson());schema9File.close();
    cad::SceneDocument migrated9;require(migrated9.load(schema9Path,&error),("Schema-v9 scene migration failed: "+error.toStdString()).c_str());
    require(migrated9.find(source)->drawingSettings.dimensionAnnotations.size()==3&&
            migrated9.find(source)->drawingSettings.dimensionAnnotations[0].axis=="X"&&
            migrated9.find(source)->drawingSettings.dimensionAnnotations[2].axis=="Z"&&
            !migrated9.find(source)->drawingSettings.partsListAnnotation.id.isEmpty()&&
            !migrated9.find(source)->drawingSettings.partsListAnnotation.visible,
            "Schema-v9 drawing sheet did not receive default persistent drawing annotations");

    QJsonObject schema10Root=root;schema10Root.insert("schema","cadworkbench.scene.v10");
    QJsonArray schema10Records=schema10Root.value("parts").toArray();
    for(int i=0;i<schema10Records.size();++i){QJsonObject item=schema10Records[i].toObject();QJsonObject settings=item.value("drawing_settings").toObject();if(!settings.isEmpty()){settings.remove("linked_parts_list");item.insert("drawing_settings",settings);}schema10Records[i]=item;}
    schema10Root.insert("parts",schema10Records);
    const QString schema10Path=dir.filePath("assembly-v10.cadscene");QFile schema10File(schema10Path);
    require(schema10File.open(QIODevice::WriteOnly),"Could not create schema-v10 linked parts-list migration fixture");
    schema10File.write(QJsonDocument(schema10Root).toJson());schema10File.close();
    cad::SceneDocument migrated10;require(migrated10.load(schema10Path,&error),("Schema-v10 scene migration failed: "+error.toStdString()).c_str());
    require(migrated10.find(source)->drawingSettings.dimensionAnnotations.size()==3&&
            migrated10.find(source)->drawingSettings.dimensionAnnotations[0].id=="dim-x"&&
            migrated10.find(source)->drawingSettings.partsListAnnotation.id.size()>0&&
            !migrated10.find(source)->drawingSettings.partsListAnnotation.visible,
            "Schema-v10 dimensions were not preserved while defaulting the linked parts list");

    QJsonObject schema5Root=root;schema5Root.insert("schema","cadworkbench.scene.v5");
    QJsonArray schema5Records=schema5Root.value("parts").toArray();
    for(int i=0;i<schema5Records.size();++i){QJsonObject item=schema5Records[i].toObject();item.remove("component_definition_id");item.remove("component_name");item.remove("occurrence_id");item.remove("placement_x");item.remove("placement_y");item.remove("placement_z");item.remove("drawing_settings");schema5Records[i]=item;}
    schema5Root.insert("parts",schema5Records);
    const QString schema5Path=dir.filePath("assembly-v5.cadscene");QFile schema5File(schema5Path);
    require(schema5File.open(QIODevice::WriteOnly),"Could not create schema-v5 migration fixture");
    require(schema5File.write(QJsonDocument(schema5Root).toJson())>0,"Could not write schema-v5 migration fixture");schema5File.close();
    cad::SceneDocument migrated;require(migrated.load(schema5Path,&error),"Schema-v5 project did not load after the assembly schema update");
    require(migrated.parts().size()==2&&migrated.parts().first().componentDefinitionId.isEmpty()&&
            migrated.parts().last().occurrenceId.isEmpty()&&migrated.parts().last().placementX==0,
            "Schema-v5 migration did not default component identity and placement cleanly");

    QJsonObject schema6Root=root;schema6Root.insert("schema","cadworkbench.scene.v6");
    QJsonArray schema6Records=schema6Root.value("parts").toArray();
    for(int i=0;i<schema6Records.size();++i){QJsonObject item=schema6Records[i].toObject();item.remove("drawing_settings");schema6Records[i]=item;}
    schema6Root.insert("parts",schema6Records);
    const QString schema6Path=dir.filePath("assembly-v6.cadscene");QFile schema6File(schema6Path);
    require(schema6File.open(QIODevice::WriteOnly),"Could not create schema-v6 migration fixture");
    require(schema6File.write(QJsonDocument(schema6Root).toJson())>0,"Could not write schema-v6 migration fixture");schema6File.close();
    cad::SceneDocument migrated6;require(migrated6.load(schema6Path,&error),"Schema-v6 project did not load after adding drawing setup storage");
    require(migrated6.parts().size()==2&&migrated6.find(source)->componentDefinitionId==definition&&
            migrated6.parts().last().placementX==50&&!migrated6.find(source)->hasDrawingSettings,
            "Schema-v6 migration did not preserve assembly identity while defaulting drawing setup");

    QJsonArray records=root.value("parts").toArray();
    QJsonObject bad=records[1].toObject();bad.insert("occurrence_id",firstOccurrence);records[1]=bad;root.insert("parts",records);
    const QString invalidScene=dir.filePath("assembly-duplicate-occurrence.cadscene");
    QFile invalid(invalidScene);require(invalid.open(QIODevice::WriteOnly),"Could not create invalid assembly fixture");
    require(invalid.write(QJsonDocument(root).toJson())>0,"Could not write invalid assembly fixture");invalid.close();
    cad::SceneDocument guarded;require(guarded.addPrimitive(cad::Primitive::Sphere,{3},&error),"Transactional-load guard fixture failed");
    const int originalCount=guarded.parts().size();
    require(!guarded.load(invalidScene,&error)&&guarded.parts().size()==originalCount,
            "Duplicate occurrence ID was accepted or partially replaced the open document");
    QJsonArray invalidDrawingRecords=QJsonDocument::fromJson(readBytes(scene)).object().value("parts").toArray();
    QJsonObject drawingRecord=invalidDrawingRecords[0].toObject();QJsonObject drawingObject=drawingRecord.value("drawing_settings").toObject();
    drawingObject.insert("issue_date","27/10/2026");drawingRecord.insert("drawing_settings",drawingObject);invalidDrawingRecords[0]=drawingRecord;
    QJsonObject invalidDrawingRoot=QJsonDocument::fromJson(readBytes(scene)).object();invalidDrawingRoot.insert("parts",invalidDrawingRecords);
    const QString invalidDrawingPath=dir.filePath("assembly-invalid-drawing.cadscene");QFile invalidDrawingFile(invalidDrawingPath);
    require(invalidDrawingFile.open(QIODevice::WriteOnly),"Could not create invalid drawing-settings fixture");
    require(invalidDrawingFile.write(QJsonDocument(invalidDrawingRoot).toJson())>0,"Could not write invalid drawing-settings fixture");invalidDrawingFile.close();
    require(!guarded.load(invalidDrawingPath,&error)&&guarded.parts().size()==originalCount,
            "Invalid saved drawing issue date was accepted or partially replaced the scene");
}
void testComponentOccurrencePatterns() {
    QString error;QTemporaryDir directory;require(directory.isValid(),"Could not create component-pattern persistence directory");
    cad::SceneDocument linear;
    require(linear.addPrimitive(cad::Primitive::Box,{10,4,2},&error),"Could not create linear-pattern component");
    const QString linearSource=linear.parts().first().id;
    require(linear.makeComponent(linearSource,"Pattern bracket",&error)&&
            linear.setPartMetadata(linearSource,"BRK-050","Patterned bracket","A",&error)&&
            linear.setMaterial(linearSource,"Aluminum 6061",2700,&error),
            "Could not prepare reusable component metadata for the linear pattern");
    const QString definition=linear.find(linearSource)->componentDefinitionId;
    require(linear.linearPatternComponentOccurrences(linearSource,4,25,0,0,&error),
            ("Could not create a linear component occurrence pattern: "+error.toStdString()).c_str());
    require(linear.parts().size()==4,"Linear component pattern did not create the requested total instance count");
    for(int index=0;index<linear.parts().size();++index){
        const cad::Part& occurrence=linear.parts()[index];
        require(occurrence.componentDefinitionId==definition&&occurrence.componentName=="Pattern bracket"&&
                !occurrence.occurrenceId.isEmpty()&&occurrence.materialName=="Aluminum 6061"&&
                occurrence.partNumber=="BRK-050",
                "Linear component pattern lost reusable identity or shared BOM/material data");
        closeTo(occurrence.placementX,25.0*index);
    }
    const auto linearBom=linear.billOfMaterials(&error);
    require(error.isEmpty()&&linearBom.size()==1&&linearBom.first().quantity==4&&
            linearBom.first().componentDefinitionId==definition,
            "Linear component-pattern occurrences did not group into one reusable BOM line");
    require(linear.undo()&&linear.parts().size()==1&&linear.redo()&&linear.parts().size()==4,
            "Linear component pattern was not one atomic undo/redo operation");
    const QString linearPath=directory.filePath("linear-component-pattern.cadscene");
    require(linear.saveAs(linearPath,&error),"Could not save the linear component pattern");
    cad::SceneDocument reopenedLinear;
    require(reopenedLinear.load(linearPath,&error)&&reopenedLinear.parts().size()==4&&
            reopenedLinear.parts().last().occurrenceId==linear.parts().last().occurrenceId&&
            reopenedLinear.parts().last().placementX==75.0,
            "Save/reopen changed patterned occurrence identities or placement");
    const int beforeInvalidLinear=linear.parts().size();error.clear();
    require(!linear.linearPatternComponentOccurrences(linearSource,1,20,0,0,&error)&&linear.parts().size()==beforeInvalidLinear,
            "Invalid linear pattern count changed the assembly");
    error.clear();require(!linear.linearPatternComponentOccurrences(linearSource,3,0,0,0,&error)&&linear.parts().size()==beforeInvalidLinear,
            "Zero-spacing linear pattern changed the assembly");

    cad::SceneDocument circular;
    require(circular.addPrimitive(cad::Primitive::Box,{10,2,2},&error),"Could not create circular-pattern component");
    const QString circularSource=circular.parts().first().id;
    require(circular.makeComponent(circularSource,"Indexing finger",&error)&&
            circular.translate(circularSource,10,0,0,&error),
            "Could not place the circular-pattern source component");
    require(circular.circularPatternComponentOccurrences(circularSource,4,0,0,0,90,&error),
            ("Could not create a circular component occurrence pattern: "+error.toStdString()).c_str());
    require(circular.parts().size()==4,"Circular component pattern did not create the requested total instance count");
    closeTo(circular.parts()[1].placementX,0,1e-8);closeTo(circular.parts()[1].placementY,10,1e-8);
    closeTo(circular.parts()[2].placementX,-10,1e-8);closeTo(circular.parts()[2].placementY,0,1e-8);
    closeTo(circular.parts()[3].placementX,0,1e-8);closeTo(circular.parts()[3].placementY,-10,1e-8);
    closeTo(circular.parts()[1].placementYawDegrees,90);closeTo(circular.parts()[3].placementYawDegrees,270);
    error.clear();
    const auto circularBom=circular.billOfMaterials(&error);
    require(error.isEmpty()&&circularBom.size()==1&&circularBom.first().quantity==4,
            ("Circular component-pattern occurrences did not aggregate in the BOM ("+
             QString::number(circularBom.size())+" rows: "+error+")").toStdString().c_str());
    require(circular.undo()&&circular.parts().size()==1&&circular.redo()&&circular.parts().size()==4,
            "Circular component pattern was not one atomic undo/redo operation");
    const QString circularPath=directory.filePath("circular-component-pattern.cadscene");
    require(circular.saveAs(circularPath,&error),"Could not save the circular component pattern");
    cad::SceneDocument reopenedCircular;
    require(reopenedCircular.load(circularPath,&error)&&reopenedCircular.parts().size()==4&&
            std::abs(reopenedCircular.parts()[1].placementY-10.0)<1e-8&&
            reopenedCircular.parts()[1].placementYawDegrees==90.0,
            "Save/reopen changed circular occurrence placement or yaw");
    const int beforeInvalidCircular=circular.parts().size();error.clear();
    require(!circular.circularPatternComponentOccurrences(circularSource,2,0,0,0,0,&error)&&
            circular.parts().size()==beforeInvalidCircular,
            "Zero-angle circular pattern changed the assembly");

    cad::SceneDocument jointed;
    require(jointed.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"Could not create jointed-pattern fixture");
    const QString jointRoot=jointed.parts().first().id;
    require(jointed.makeComponent(jointRoot,"Pattern root",&error)&&
            jointed.insertComponentOccurrence(jointRoot,15,0,0,&error),
            "Could not create the jointed-pattern occurrences");
    const QString jointChild=jointed.parts().last().id;
    require(jointed.addRigidJoint(jointRoot,jointChild,&error),"Could not create the jointed-pattern guard fixture");
    const int jointedCount=jointed.parts().size();error.clear();
    require(!jointed.linearPatternComponentOccurrences(jointRoot,3,10,0,0,&error)&&
            !jointed.circularPatternComponentOccurrences(jointChild,3,0,0,0,90,&error)&&
            jointed.parts().size()==jointedCount,
            "Component patterns accepted a jointed subassembly or partially changed it");
}

void testSelectedEdgeDrawingDimension() {
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Could not create drawing-edge fixture");
    const QString id=doc.parts().first().id;
    const cad::Part original=*doc.find(id);
    double measured=0.0;
    require(doc.measureEdgeLength({id,original.geometryToken,1},&measured,&error),"Could not measure drawing source edge");
    cad::DrawingSheetSettings settings;
    settings.edgeDimensionAnnotations.append({{},id,original.geometryToken,1,"TOP",12.0});
    require(doc.setDrawingSheetSettings(id,settings,&error),("Could not save linked edge dimension: "+error.toStdString()).c_str());
    const auto saved=doc.find(id)->drawingSettings;
    require(saved.edgeDimensionAnnotations.size()==1&&!saved.edgeDimensionAnnotations.first().id.isEmpty(),
            "Linked drawing edge dimension lost its generated persistent ID");
    QByteArray svg;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&svg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {},saved.sheetId,saved.noteAnnotations,saved.dimensionAnnotations,saved.partsListAnnotation,
            {},saved.sourceBodyIds,{}, {}, {},saved.edgeDimensionAnnotations,{*doc.find(id)}),
            ("Could not render linked edge length: "+error.toStdString()).c_str());
    require(svg.contains("data-drawing-annotation=\"selected-edge-dimension\"")&&
            svg.contains("data-source-body-id=\""+id.toUtf8()+"\"")&&
            svg.contains("data-value-mm=\""+QByteArray::number(measured,'f',3)+"\""),
            "Drawing edge length did not retain its source or measured value");
    QTemporaryDir directory;require(directory.isValid(),"Could not create edge-dimension test directory");
    const QString scene=directory.filePath("edge-dimension.cadscene");
    require(doc.saveAs(scene,&error),"Could not save edge-dimension scene");
    cad::SceneDocument loaded;require(loaded.load(scene,&error),("Could not reopen edge-dimension scene: "+error.toStdString()).c_str());
    require(loaded.find(id)->drawingSettings.edgeDimensionAnnotations==saved.edgeDimensionAnnotations,
            "Save/reopen changed the linked edge dimension");
    require(loaded.translate(id,4,0,0,&error),"Could not edit linked drawing source geometry");
    QByteArray staleSvg;error.clear();
    require(!cad::buildOrthographicDrawingSvg(*loaded.find(id),&staleSvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {},saved.sheetId,saved.noteAnnotations,saved.dimensionAnnotations,saved.partsListAnnotation,
            {},saved.sourceBodyIds,{}, {}, {},saved.edgeDimensionAnnotations,{*loaded.find(id)})&&error.contains("changed"),
            "Drawing accepted an edge reference after the source topology revision changed");
    auto revised=loaded.find(id)->drawingSettings;
    revised.edgeDimensionAnnotations.first().sourceGeometryToken=loaded.find(id)->geometryToken;
    require(loaded.setDrawingSheetSettings(id,revised,&error),"Could not relink changed edge dimension");
    revised.edgeDimensionAnnotations.first().edgeIndex=99999;error.clear();
    require(!cad::buildOrthographicDrawingSvg(*loaded.find(id),&staleSvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {},saved.sheetId,saved.noteAnnotations,saved.dimensionAnnotations,saved.partsListAnnotation,
            {},saved.sourceBodyIds,{}, {}, {},revised.edgeDimensionAnnotations,{*loaded.find(id)}),
            "Drawing accepted an out-of-range linked edge index");
}

void testOrthographicDrawingExport() {
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Drawing fixture creation failed");
    const QString id=doc.parts().first().id;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Second drawing source fixture creation failed");
    const QString secondId=doc.parts().last().id;require(doc.translate(secondId,50,0,0,&error),"Could not place the second drawing source body");
    require(doc.setPartMetadata(id,"PLATE-001","Flange & support <upper>","B",&error),"Drawing metadata setup failed");
    require(doc.setMaterial(id,"Aluminum 6061",2700,&error),"Drawing material setup failed");
    QByteArray svg;double scale=0;int visible=0,hidden=0;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&svg,&error,&scale,&visible,&hidden),
            ("Orthographic drawing generation failed: "+error.toStdString()).c_str());
    require(svg.startsWith("<?xml")&&svg.contains("width=\"297mm\"")&&svg.contains("height=\"210mm\""),
            "Drawing export is not an A4 landscape SVG");
    QByteArray drawingDxf;
    const bool convertedDrawingDxf=cad::buildOrthographicDrawingDxf(svg,&drawingDxf,&error);
    require(convertedDrawingDxf,("Drawing DXF conversion failed: "+error.toStdString()).c_str());
    require(drawingDxf.contains("$INSUNITS\n70\n4")&&drawingDxf.contains("0\nLINE\n8\nGEOMETRY")&&
            drawingDxf.contains("0\nTEXT\n")&&drawingDxf.contains("0\nCIRCLE\n")&&drawingDxf.endsWith("0\nEOF\n"),
            "Drawing DXF omitted millimetre units, editable geometry, text, circle, or EOF");
    const QByteArray validDrawingDxf=drawingDxf;
    require(!cad::buildOrthographicDrawingDxf("<svg/>",&drawingDxf,&error)&&drawingDxf==validDrawingDxf,
            "Invalid input changed the drawing DXF output");
    const QVector<cad::DrawingDetailAnnotation> detailViews={{"detail-front","FRONT",0,10,5,245,80,18}};
    QByteArray detailSvg;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&detailSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},detailViews),
            ("Drawing detail view was rejected: "+error.toStdString()).c_str());
    QByteArray detailDxf;
    require(cad::buildOrthographicDrawingDxf(detailSvg,&detailDxf,&error)&&
            detailDxf.contains("0\nLINE\n8\nDETAIL_VIEW\n"),
            "DXF detail view omitted clipped editable linework");
    require(detailSvg.contains("data-drawing-annotation=\"detail-view\"")&&
            detailSvg.contains("data-source-view=\"FRONT\"")&&detailSvg.contains("data-detail-boundary=")&&
            detailSvg.contains("clip-path=\"url(#detail-")&&detailSvg.contains(">DETAIL A</text>"),
            "Detail view is missing its selectable source, capture boundary, clipped enlargement, or label");
    QVector<cad::DrawingDetailAnnotation> invalidDetails=detailViews;invalidDetails[0].sourceXmm=1000;
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&detailSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},invalidDetails),
            "Detail view centered outside its source view was accepted");
    const QVector<cad::DrawingSectionAnnotation> sectionViews={{"section-x","X",0.35,245,75},
        {"section-y","Y",0.45,245,125},{"section-z","Z",0.55,235,175}};
    QByteArray independentSectionSvg;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&independentSectionSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},{},sectionViews),
            ("Independent section view was rejected: "+error.toStdString()).c_str());
    require(independentSectionSvg.contains("data-drawing-annotation=\"section-cut\" data-annotation-id=\"section-x\"")&&
            independentSectionSvg.contains("data-depth-fraction=\"0.3500\" data-plane-axis=\"X\" data-source-view=\"TOP\"")&&
            independentSectionSvg.contains("data-depth-fraction=\"0.4500\" data-plane-axis=\"Y\" data-source-view=\"TOP\"")&&
            independentSectionSvg.contains("data-depth-fraction=\"0.5500\" data-plane-axis=\"Z\" data-source-view=\"FRONT\"")&&
            independentSectionSvg.contains("data-view-label=\"SECTION A-A\"")&&
            independentSectionSvg.contains("data-view-label=\"SECTION B-B\"")&&
            independentSectionSvg.contains("data-view-label=\"SECTION C-C\"")&&
            independentSectionSvg.contains("data-section-label=\"section-z\""),
            "Independent X/Y/Z sections are missing cut identity, depth, projected source, hatching, or callouts");
    QVector<cad::DrawingSectionAnnotation> invalidSections=sectionViews;invalidSections[0].depthFraction=1.0;
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&independentSectionSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},{},invalidSections),
            "Independent section outside its valid cutting-depth range was accepted");
    QTemporaryDir pdfDir;require(pdfDir.isValid(),"Drawing PDF test folder could not be created");
    const QString a4PdfPath=pdfDir.filePath("drawing-a4.pdf");
    require(cad::exportOrthographicDrawingPdf(svg,a4PdfPath,cad::DrawingSheetSize::A4Landscape,&error),
            ("A4 drawing PDF export failed: "+error.toStdString()).c_str());
    const QByteArray a4Pdf=readBytes(a4PdfPath);
    require(a4Pdf.startsWith("%PDF-")&&a4Pdf.contains("%%EOF")&&a4Pdf.size()>1000,
            "A4 drawing PDF is incomplete or empty");
    const QString a3PdfPath=pdfDir.filePath("drawing-a3.pdf");
    require(cad::exportOrthographicDrawingPdf(svg,a3PdfPath,cad::DrawingSheetSize::A3Landscape,&error),
            ("A3 drawing PDF export failed: "+error.toStdString()).c_str());
    require(readBytes(a3PdfPath).startsWith("%PDF-"),"A3 landscape drawing PDF was not created");
    const QString protectedPdfPath=pdfDir.filePath("preserved.pdf");writeBytes(protectedPdfPath,"keep-existing-file");
    require(!cad::exportOrthographicDrawingPdf("<not svg>",protectedPdfPath,cad::DrawingSheetSize::A4Landscape,&error)&&
            readBytes(protectedPdfPath)=="keep-existing-file",
            "Malformed SVG PDF export succeeded or damaged the existing destination file");
    require(svg.contains("data-cadworkbench-source-body-id=\""+id.toUtf8()+"\"")&&
            svg.contains("data-cadworkbench-source-geometry-token=\""+doc.find(id)->geometryToken.toUtf8()+"\"")&&
            svg.contains("data-cadworkbench-link-status=\"export-snapshot\"")&&svg.contains("data-view-label=\"FRONT\"")&&
            svg.contains("data-drawing-annotation=\"overall-dimension\" data-axis=\"X\" data-annotation-id="),
            "Drawing SVG omitted its model-source snapshot identity or selectable view grouping");
    require(svg.contains(">TOP</text>")&&svg.contains(">FRONT</text>")&&svg.contains(">RIGHT</text>")
            &&svg.contains(">ISOMETRIC</text>"),
            "Orthographic and isometric drawing views are not labelled");
    require(svg.contains("data-projection-symbol=\"third-angle\"")&&svg.contains("<circle cx=\"261\" cy=\"165\" r=\"3\""),
            "Third-angle projection symbol or end-view placement is missing");
    require(svg.contains("X 40.000 mm")&&svg.contains("Z 20.000 mm")&&svg.contains("Y 30.000 mm")
            &&svg.contains("OVERALL ENVELOPE DIMENSIONS ONLY"),
            "Overall X/Y/Z envelope dimensions do not match the orthographic views");
    require(svg.contains("Flange &amp; support &lt;upper&gt;")&&svg.contains("PLATE-001")&&svg.contains("Aluminum 6061"),
            "Drawing title block omitted or failed to escape part metadata");
    QByteArray notesSvg;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&notesSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{"Inspect & deburr <edges>","Refer to assembly 42"}),
            ("Drawing general notes were rejected: "+error.toStdString()).c_str());
    require(notesSvg.contains("GENERAL NOTES")&&notesSvg.contains("Inspect &amp; deburr &lt;edges&gt;")&&notesSvg.contains("Refer to assembly 42"),
            "General notes were omitted or not safely XML-escaped");
    require(notesSvg.contains("data-drawing-annotation=\"general-notes\""),
            "General notes were not grouped as a selectable drawing annotation");
    cad::DrawingSheetSettings placedSheet;placedSheet.viewCenters.insert("FRONT",QPointF(111,94));
    placedSheet.generalNotes={"Check fit & finish"};
    placedSheet.noteAnnotations={{"note-fit-001","Check fit & finish",33,72}};
    placedSheet.dimensionAnnotations={{"dimension-x-01","X",12.0},{"dimension-y-01","Y",14.0},{"dimension-z-01","Z",16.0}};
    placedSheet.partsListAnnotation={"bom-table-01",true,12,12};
    require(doc.setDrawingSheetSettings(id,placedSheet,&error),"Could not store placed drawing view and note fixture");
    const cad::Part* placedDrawingPart=doc.find(id);const auto& placedSettings=placedDrawingPart->drawingSettings;
    cad::BillOfMaterialsRow drawingBomRow;drawingBomRow.itemNumber=4;drawingBomRow.partNumber="PUMP-004";
    drawingBomRow.description="Pump & bracket";drawingBomRow.revision="B";drawingBomRow.materialName="Aluminum";
    drawingBomRow.quantity=2;drawingBomRow.unitMassKg=0.024;drawingBomRow.totalMassKg=0.048;drawingBomRow.componentDefinitionId="component-pump-004";
    const QVector<cad::BillOfMaterialsRow> drawingBomRows={drawingBomRow};
    QByteArray placedSheetSvg;
    require(cad::buildOrthographicDrawingSvg(*placedDrawingPart,&placedSheetSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,placedSettings.generalNotes,{},placedSettings.viewCenters,
                placedSettings.viewIds,placedSettings.sheetId,placedSettings.noteAnnotations,placedSettings.dimensionAnnotations,
                placedSettings.partsListAnnotation,drawingBomRows),
            "Saved drawing placements, notes, dimensions, or linked BOM table could not be exported");
    QByteArray frontGroup("data-view-label=\"FRONT\" data-view-id=\"");frontGroup+=placedSettings.viewIds.value("FRONT").toUtf8();
    frontGroup+="\" data-view-center-x=\"111.000\" data-view-center-y=\"94.000\"";
    QByteArray sheetIdAttribute("data-cadworkbench-sheet-id=\"");sheetIdAttribute+=placedSettings.sheetId.toUtf8();sheetIdAttribute+='"';
    require(placedSheetSvg.contains(sheetIdAttribute)&&placedSheetSvg.contains(frontGroup),
            "SVG omitted persistent sheet/view IDs or the saved front-view center");
    require(placedSheetSvg.contains("data-annotation-id=\"note-fit-001\" x=\"33.000\" y=\"72.000\""),
            "SVG omitted the saved note identity or position");
    require(placedSheetSvg.contains("data-axis=\"X\" data-annotation-id=\"dimension-x-01\" data-offset-mm=\"12.000\""),
            "SVG omitted the persistent dimension identity or editable placement");
    require(placedSheetSvg.contains("data-drawing-annotation=\"linked-parts-list\" data-annotation-id=\"bom-table-01\"")&&
            placedSheetSvg.contains("data-bom-item-number=\"4\" data-component-definition-id=\"component-pump-004\"")&&
            placedSheetSvg.contains("PUMP-004")&&placedSheetSvg.contains("Pump &amp; bracket")&&
            placedSheetSvg.contains("Aluminum")&&placedSheetSvg.contains("MASS (kg)")&&
            placedSheetSvg.contains("data-bom-unit-mass-kg=\"0.024\"")&&placedSheetSvg.contains(">0.024</text>")&&
            placedSheetSvg.contains("data-source=\"drawing-source-bom\""),
            "SVG omitted the BOM unit-mass column/value, mis-escaped the linked row, or lost its stable table/source identity");
    require(placedSheetSvg.contains(">Check fit &amp; finish</text>"),"SVG omitted or failed to escape the saved note text");
    QByteArray titleBlockSvg;
    const cad::DrawingTitleBlock titleBlock{"DWG-007","Hamza & Co","Reviewer <2>","2026-10-27","C&1"};
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&titleBlockSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{},titleBlock),
            "Custom drawing title-block fields were rejected");
    require(titleBlockSvg.contains("DWG-007")&&titleBlockSvg.contains("Hamza &amp; Co")&&
            titleBlockSvg.contains("Reviewer &lt;2&gt;")&&titleBlockSvg.contains("2026-10-27")&&titleBlockSvg.contains("C&amp;1"),
            "Custom drawing number, author, checker, revision, or issue date was omitted or not XML-escaped");
    require(cad::checkDrawingSvg(titleBlockSvg,cad::StandardsProfile::IsoDrawingReadiness).warnings==0,
            "Complete custom title-block drawing did not pass ISO structural screen");
    cad::DrawingTitleBlock invalidIssueDate=titleBlock;invalidIssueDate.issueDate="27/10/2026";
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&titleBlockSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{},invalidIssueDate),
            "Non-ISO drawing issue date was accepted by the exporter");
    QByteArray malformedDateSvg;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&malformedDateSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{},titleBlock),
            "Valid ISO drawing issue date could not be generated for readiness validation");
    malformedDateSvg.replace("2026-10-27","27/10/2026");
    const auto malformedDateCheck=cad::checkDrawingSvg(malformedDateSvg,cad::StandardsProfile::AsmeDrawingReadiness);
    require(std::any_of(malformedDateCheck.findings.cbegin(),malformedDateCheck.findings.cend(),
                        [](const auto& f){return f.code=="DRAWING_ISSUE_DATE_FORMAT";}),
            "Drawing readiness screen did not detect a malformed issue date");
    cad::Part noDrawingNumber=*doc.find(id);noDrawingNumber.partNumber.clear();
    QByteArray missingDrawingNumberSvg;
    require(cad::buildOrthographicDrawingSvg(noDrawingNumber,&missingDrawingNumberSvg,&error),
            "Drawing without a part or drawing number could not be generated for the missing-field check");
    const auto missingDrawingNumberCheck=cad::checkDrawingSvg(missingDrawingNumberSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(missingDrawingNumberCheck.findings.cbegin(),missingDrawingNumberCheck.findings.cend(),
                        [](const auto& f){return f.code=="DRAWING_NUMBER_MISSING";}),
            "Drawing readiness screen did not detect an absent drawing number");
    cad::DrawingTitleBlock oversizedTitle=titleBlock;oversizedTitle.drawingNumber=QString(33,'D');
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&titleBlockSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{},oversizedTitle),
            "Overlong title-block drawing number was accepted");
    oversizedTitle=titleBlock;oversizedTitle.revision=QString(17,'R');
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&titleBlockSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{},oversizedTitle),
            "Overlong title-block revision was accepted");
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&notesSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{"a","b","c","d","e","f"}),
            "More than five general drawing notes were accepted");
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&notesSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{QString(61,'x')}),
            "An overlong general drawing note was accepted");
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&notesSvg,&error,nullptr,nullptr,nullptr,
                cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
                cad::DrawingAuxiliaryView::Isometric,0.5,{QStringLiteral("bad\tcontrol")}),
            "A general drawing note with a control character was accepted");
    require(visible>0&&scale>0.0&&scale<=1.0,"Drawing produced no visible linework or an invalid automatic scale");
    const auto isoDrawingCheck=cad::checkDrawingSvg(svg,cad::StandardsProfile::IsoDrawingReadiness);
    require(isoDrawingCheck.errors==0&&isoDrawingCheck.warnings==0,
            "Complete drawing did not pass the ISO structural-readiness screen");
    const auto asmeDrawingCheck=cad::checkDrawingSvg(svg,cad::StandardsProfile::AsmeDrawingReadiness);
    require(asmeDrawingCheck.errors==0&&asmeDrawingCheck.warnings==0
            &&std::any_of(asmeDrawingCheck.findings.cbegin(),asmeDrawingCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_CHECK_SCOPE"&&f.message.contains("not standards certification");}),
            "ASME structural screen failed or omitted its no-certification scope note");
    cad::DrawingReadinessPolicy strictPolicy;
    strictPolicy.requiredTitleBlockFields={"drawing_number","drawn_by","checked_by","issue_date","revision","material"};
    strictPolicy.requiredAnnotationConventions={"envelope_dimensions","projection_symbol","auxiliary_view"};
    const auto strictPass=cad::checkDrawingSvg(titleBlockSvg,cad::StandardsProfile::IsoDrawingReadiness,strictPolicy);
    require(strictPass.errors==0,"Configured drawing checklist rejected a complete title block and annotations");
    const auto strictFailure=cad::checkDrawingSvg(svg,cad::StandardsProfile::IsoDrawingReadiness,strictPolicy);
    require(std::any_of(strictFailure.findings.cbegin(),strictFailure.findings.cend(),[](const auto& finding){return finding.code=="DRAWING_POLICY_REQUIRED_FIELD_MISSING";}),
            "Configured drawing checklist did not make required title-block fields enforceable");
    const auto currentSourceCheck=cad::checkDrawingSourceSnapshot(*doc.find(id),svg);
    require(currentSourceCheck.errors==0&&currentSourceCheck.warnings==0&&
            std::any_of(currentSourceCheck.findings.cbegin(),currentSourceCheck.findings.cend(),[](const auto& finding){return finding.code=="DRAWING_SOURCE_CURRENT";}),
            "Model-linked snapshot identity did not report the current source geometry");
    cad::Part wrongSource=*doc.find(id);wrongSource.id="different-body";
    require(cad::checkDrawingSourceSnapshot(wrongSource,svg).errors==1,"Drawing source check accepted a different selected body");
    cad::Part staleSource=*doc.find(id);staleSource.geometryToken="changed-geometry-token";
    const auto staleCheck=cad::checkDrawingSourceSnapshot(staleSource,svg);
    require(staleCheck.errors==0&&staleCheck.warnings==1&&
            std::any_of(staleCheck.findings.cbegin(),staleCheck.findings.cend(),[](const auto& finding){return finding.code=="DRAWING_SOURCE_GEOMETRY_STALE";}),
            "Drawing source check failed to detect a stale geometry snapshot");
    require(cad::checkDrawingSourceSnapshot(*doc.find(id),"<svg><path></svg>").errors==1,
            "Drawing source check accepted a malformed SVG document");
    QByteArray conflictingProjectionSvg=svg;
    conflictingProjectionSvg.replace("data-projection-symbol=\"third-angle\"","data-projection-symbol=\"first-angle\"");
    const auto conflictingProjectionCheck=cad::checkDrawingSvg(conflictingProjectionSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(conflictingProjectionCheck.findings.cbegin(),conflictingProjectionCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_PROJECTION_SYMBOL_CONFLICT";}),
            "Drawing screen failed to detect a projection-heading/symbol mismatch");
    QByteArray misplacedProjectionSvg=svg;
    misplacedProjectionSvg.replace("<circle cx=\"261\" cy=\"165\" r=\"3\"/>","<circle cx=\"282\" cy=\"165\" r=\"3\"/>");
    const auto misplacedProjectionCheck=cad::checkDrawingSvg(misplacedProjectionSvg,cad::StandardsProfile::AsmeDrawingReadiness);
    require(std::any_of(misplacedProjectionCheck.findings.cbegin(),misplacedProjectionCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_PROJECTION_SYMBOL_PLACEMENT";}),
            "Drawing screen failed to detect a circular projection view on the wrong side");
    QByteArray misplacedViewSvg=svg;
    const qsizetype rightTextEnd=misplacedViewSvg.indexOf(">RIGHT</text>");
    const qsizetype rightTextStart=rightTextEnd<0 ? -1 : misplacedViewSvg.lastIndexOf("<text ",rightTextEnd);
    const qsizetype rightXStart=rightTextStart<0 ? -1 : misplacedViewSvg.indexOf("x=\"",rightTextStart);
    const qsizetype rightXEnd=rightXStart<0 ? -1 : misplacedViewSvg.indexOf('"',rightXStart+3);
    require(rightXEnd>rightXStart&&rightXEnd>rightTextStart&&rightTextEnd>rightTextStart,
            "Could not locate the RIGHT view label for projection-layout regression");
    misplacedViewSvg.replace(rightXStart+3,rightXEnd-rightXStart-3,"45.00");
    const auto misplacedViewCheck=cad::checkDrawingSvg(misplacedViewSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(misplacedViewCheck.findings.cbegin(),misplacedViewCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_PROJECTION_LAYOUT";}),
            "Drawing screen failed to detect third-angle orthographic views placed using the wrong convention");
    QByteArray invalidDimensionSvg=svg;invalidDimensionSvg.replace("X 40.000 mm","X NaN mm");
    const auto invalidDimensionCheck=cad::checkDrawingSvg(invalidDimensionSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(invalidDimensionCheck.findings.cbegin(),invalidDimensionCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_ENVELOPE_DIMENSION_MISSING"&&f.message.contains("X");}),
            "Drawing screen accepted a non-numeric envelope dimension");
    QByteArray missingProjectionSymbolSvg=svg;missingProjectionSymbolSvg.replace("data-projection-symbol=\"third-angle\"","data-schematic-symbol=\"third-angle\"");
    const auto missingProjectionSymbolCheck=cad::checkDrawingSvg(missingProjectionSymbolSvg,cad::StandardsProfile::AsmeDrawingReadiness);
    require(std::any_of(missingProjectionSymbolCheck.findings.cbegin(),missingProjectionSymbolCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_PROJECTION_SYMBOL_MISSING";}),
            "Drawing screen failed to warn when the schematic projection marker was absent");
    QByteArray missingViewSvg=svg;missingViewSvg.replace(">FRONT</text>",">WEST</text>");
    const auto missingViewCheck=cad::checkDrawingSvg(missingViewSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(missingViewCheck.errors==1&&std::any_of(missingViewCheck.findings.cbegin(),missingViewCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_VIEW_MISSING";}),
            "Drawing screen did not report a missing required orthographic view");
    require(cad::checkDrawingSvg("<not svg>",cad::StandardsProfile::IsoDrawingReadiness).errors==1,
            "Drawing screen accepted malformed SVG");
    QByteArray mismatchedViewBoxSvg=svg;mismatchedViewBoxSvg.replace("viewBox=\"0 0 297 210\"","viewBox=\"0 0 296 210\"");
    const auto mismatchedViewBoxCheck=cad::checkDrawingSvg(mismatchedViewBoxSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(mismatchedViewBoxCheck.findings.cbegin(),mismatchedViewBoxCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_SHEET_FORMAT";}),
            "Drawing screen accepted a viewBox that conflicts with the physical sheet dimensions");
    require(cad::checkDrawingSvg(svg,cad::StandardsProfile::BomData).errors==1,
            "Drawing SVG screen accepted the BOM-only profile");
    cad::SceneDocument cylinderDoc;
    require(cylinderDoc.addPrimitive(cad::Primitive::Cylinder,{5,12},&error),"Centerline cylinder fixture creation failed");
    QByteArray centerlineSvg;
    require(cad::buildOrthographicDrawingSvg(cylinderDoc.parts().first(),&centerlineSvg,&error),
            "Cylindrical drawing generation failed");
    const auto incompleteDrawingCheck=cad::checkDrawingSvg(centerlineSvg,cad::StandardsProfile::IsoDrawingReadiness);
    require(std::any_of(incompleteDrawingCheck.findings.cbegin(),incompleteDrawingCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_PART_NUMBER_MISSING";}),
            "Drawing screen did not flag missing title-block metadata");
    require(centerlineSvg.count("stroke-dasharray=\"6,1,1,1\"")>=5,
            "Cylindrical face axes and end-view center marks were not added to the drawing");
    require(centerlineSvg.contains(QString::fromUtf8("Ø10.000 mm").toUtf8()),
            "End-on cylindrical feature did not receive its true-diameter drawing callout");
    auto svgAttribute=[](const QByteArray& data,const QByteArray& attribute){
        const QByteArray marker=attribute+"=\"";const int start=data.indexOf(marker);if(start<0)return QByteArray{};
        const int valueStart=start+marker.size(),end=data.indexOf('"',valueStart);return end<0?QByteArray{}:data.mid(valueStart,end-valueStart);
    };
    const QByteArray cylinderFeatureReference=svgAttribute(centerlineSvg,"data-feature-reference");
    const QByteArray cylinderDimensionId=svgAttribute(centerlineSvg,"data-annotation-id");
    require(centerlineSvg.contains("data-drawing-annotation=\"feature-linked-dimension\"")&&
            centerlineSvg.contains("data-feature-type=\"cylindrical-diameter\"")&&
            centerlineSvg.contains("data-parameter=\"diameter\"")&&!cylinderFeatureReference.isEmpty()&&!cylinderDimensionId.isEmpty(),
            "Cylindrical diameter callout did not retain its selectable feature and parameter identity");
    QByteArray repeatedCylinderSvg;
    require(cad::buildOrthographicDrawingSvg(cylinderDoc.parts().first(),&repeatedCylinderSvg,&error)&&
            svgAttribute(repeatedCylinderSvg,"data-feature-reference")==cylinderFeatureReference&&
            svgAttribute(repeatedCylinderSvg,"data-annotation-id")==cylinderDimensionId,
            "Re-export changed a feature-dimension identity for unchanged geometry");
    require(cylinderDoc.translate(cylinderDoc.parts().first().id,5,0,0,&error),"Could not move the linked-dimension source cylinder");
    QByteArray movedCylinderSvg;require(cad::buildOrthographicDrawingSvg(cylinderDoc.parts().first(),&movedCylinderSvg,&error),
            "Moved cylindrical source could not regenerate its feature-linked dimension");
    require(svgAttribute(movedCylinderSvg,"data-feature-reference")!=cylinderFeatureReference,
            "Feature-linked drawing reference did not follow its changed source geometry");
    QXmlStreamReader reader(svg);
    while(!reader.atEnd()) reader.readNext();
    require(!reader.hasError(),"Drawing SVG is not well-formed XML");
    auto viewLabelY=[](const QByteArray& data,const QString& label) {
        QXmlStreamReader xmlReader(data);
        while(!xmlReader.atEnd()) {
            xmlReader.readNext();
            if(xmlReader.isStartElement()&&xmlReader.name()=="text") {
                const double y=xmlReader.attributes().value("y").toDouble();
                if(xmlReader.readElementText()==label)return y;
            }
        }
        return -1.0;
    };
    auto viewLabelX=[](const QByteArray& data,const QString& label) {
        QXmlStreamReader xmlReader(data);
        while(!xmlReader.atEnd()) {
            xmlReader.readNext();
            if(xmlReader.isStartElement()&&xmlReader.name()=="text") {
                const double x=xmlReader.attributes().value("x").toDouble();
                if(xmlReader.readElementText()==label)return x;
            }
        }
        return -1.0;
    };
    require(viewLabelX(svg,"ISOMETRIC")>viewLabelX(svg,"RIGHT")
            &&viewLabelY(svg,"TOP")<viewLabelY(svg,"ISOMETRIC")
            &&viewLabelY(svg,"ISOMETRIC")<viewLabelY(svg,"FRONT"),
            "Third-angle isometric view was not placed in its reserved sheet quadrant");
    QByteArray firstAngle;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&firstAngle,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::FirstAngle),"First-angle SVG layout generation failed");
    require(firstAngle.contains("FIRST-ANGLE TECHNICAL DRAWING")&&firstAngle.contains("<rect x=\"10\" y=\"171\"")
            &&firstAngle.contains("data-projection-symbol=\"first-angle\"")
            &&firstAngle.contains("<circle cx=\"282\" cy=\"165\" r=\"3\""),
            "First-angle convention label or title-block placement is missing");
    const auto firstAngleIsoCheck=cad::checkDrawingSvg(firstAngle,cad::StandardsProfile::IsoDrawingReadiness);
    const auto firstAngleAsmeCheck=cad::checkDrawingSvg(firstAngle,cad::StandardsProfile::AsmeDrawingReadiness);
    require(firstAngleIsoCheck.errors==0&&firstAngleIsoCheck.warnings==0
            &&firstAngleAsmeCheck.errors==0&&firstAngleAsmeCheck.warnings==0,
            "A valid first-angle drawing did not pass both structural-readiness profiles");
    require(viewLabelX(firstAngle,"ISOMETRIC")>viewLabelX(firstAngle,"RIGHT"),
            "First-angle isometric view was not placed opposite the right-side view");
    QByteArray sectionSvg;int sectionVisible=0;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&sectionSvg,&error,nullptr,&sectionVisible,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
            cad::DrawingAuxiliaryView::SectionA),
            ("Mid-depth section drawing generation failed: "+error.toStdString()).c_str());
    require(sectionSvg.contains(">SECTION A-A</text>")&&sectionSvg.contains("CUT PLANE A-A AT MID-DEPTH")
            &&sectionSvg.contains("stroke-width=\"0.12\"")&&sectionVisible>=4,
            "Section view, hatching, cutting-plane callout, or section contour is missing");
    QByteArray positionedSection;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&positionedSection,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
            cad::DrawingAuxiliaryView::SectionA,0.35),
            "Custom-depth section generation failed");
    require(positionedSection.contains("CUT PLANE A-A AT 35% DEPTH (Y=10.500 mm)"),
            "Custom section plane did not report its chosen depth and model coordinate");
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),&positionedSection,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,
            cad::DrawingAuxiliaryView::SectionA,1.0),
            "Boundary-only section plane was accepted");
    require(viewLabelY(svg,"TOP")<viewLabelY(svg,"FRONT")&&viewLabelY(firstAngle,"TOP")>viewLabelY(firstAngle,"FRONT"),
            "First-angle and third-angle top-view placements were not reversed");
    QByteArray a3Svg;double a3Scale=0.0;
    require(cad::buildOrthographicDrawingSvg(*doc.find(id),&a3Svg,&error,&a3Scale,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A3Landscape),
            "A3 landscape SVG generation failed");
    require(a3Svg.contains("width=\"420mm\"")&&a3Svg.contains("height=\"297mm\"")
            &&a3Svg.contains("scale(1.414141)")&&a3Scale>1.4,
            "A3 sheet dimensions or proportional drawing scale are incorrect");
    QByteArray a3Dxf;
    require(cad::buildOrthographicDrawingDxf(a3Svg,&a3Dxf,&error)&&a3Dxf.contains("0\nLINE\n")&&
            a3Dxf.contains("$INSUNITS\n70\n4"),"A3 drawing failed millimetre DXF conversion");
    QXmlStreamReader a3Reader(a3Svg);while(!a3Reader.atEnd())a3Reader.readNext();
    require(!a3Reader.hasError(),"A3 SVG is not well-formed XML");
    const auto a3IsoCheck=cad::checkDrawingSvg(a3Svg,cad::StandardsProfile::IsoDrawingReadiness);
    const auto a3AsmeCheck=cad::checkDrawingSvg(a3Svg,cad::StandardsProfile::AsmeDrawingReadiness);
    require(a3IsoCheck.errors==0&&a3IsoCheck.warnings==0&&a3AsmeCheck.errors==0&&a3AsmeCheck.warnings==0,
            "Valid scaled A3 third-angle sheet did not pass ISO and ASME structural-readiness checks");
    require(!cad::buildOrthographicDrawingSvg(*doc.find(id),nullptr,&error),"Null drawing output buffer was accepted");
    QTemporaryDir dir;require(dir.isValid(),"Drawing test folder creation failed");
    const QString path=dir.filePath("plate.svg");
    require(cad::exportOrthographicDrawingSvg(*doc.find(id),path,&error),"Orthographic SVG save failed");
    require(readBytes(path)==svg,"Saved drawing SVG differs from generated drawing data");
    const QString dxfPath=dir.filePath("plate.dxf");
    require(cad::exportOrthographicDrawingDxf(svg,dxfPath,&error)&&readBytes(dxfPath)==validDrawingDxf,
            "Saved drawing DXF differs from its generated snapshot");
    require(!cad::exportOrthographicDrawingSvg(*doc.find(id),{},&error),"Empty drawing path was accepted");
    cad::Part assemblyDrawing;
    require(cad::buildDrawingAssemblyPart({*doc.find(id),*doc.find(secondId)},&assemblyDrawing,&error),
            ("Multi-body drawing source preparation failed: "+error.toStdString()).c_str());
    Bnd_Box assemblyBounds;BRepBndLib::Add(assemblyDrawing.shape,assemblyBounds);
    double minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;assemblyBounds.Get(minX,minY,minZ,maxX,maxY,maxZ);
    closeTo(volume(assemblyDrawing.shape),25000);closeTo(minX,0);closeTo(maxX,60);closeTo(maxY,30);closeTo(maxZ,20);
    require(assemblyDrawing.geometryToken.size()==64&&assemblyDrawing.description.contains("2 bodies"),
            "Multi-body drawing source omitted its aggregate revision identity or assembly title");
    const auto assemblyDrawingBom=doc.billOfMaterials(QStringList{id,secondId},&error);
    int balloonItemNumber=0;for(const auto& row:assemblyDrawingBom)if(row.bodyIds.contains(id))balloonItemNumber=row.itemNumber;
    QByteArray assemblySvg;
    require(cad::buildOrthographicDrawingSvg(assemblyDrawing,&assemblySvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {}, {}, {}, {}, {},assemblyDrawingBom,{id,secondId},{{"balloon-source-1",id,250,170}}),
            ("Multi-body drawing projection failed: "+error.toStdString()).c_str());
    QStringList canonicalSourceIds{id,secondId};canonicalSourceIds.sort(Qt::CaseSensitive);
    const QByteArray sourceSetAttribute="data-cadworkbench-source-body-ids=\""+canonicalSourceIds.join(',').toUtf8()+"\"";
    require(assemblySvg.contains("data-cadworkbench-source-body-id=\""+assemblyDrawing.id.toUtf8()+"\"")&&
            assemblySvg.contains(sourceSetAttribute),
            "Drawing singular source identity or canonical source-set metadata is incorrect");
    QByteArray reversedAssemblySvg;
    require(cad::buildOrthographicDrawingSvg(assemblyDrawing,&reversedAssemblySvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {}, {}, {}, {}, {},assemblyDrawingBom,{secondId,id}),
            "Drawing projection rejected a reordered valid source set");
    require(reversedAssemblySvg.contains(sourceSetAttribute),
            "Drawing source-set metadata changed when the same source IDs were reordered");
    require(!cad::buildOrthographicDrawingSvg(assemblyDrawing,&reversedAssemblySvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {}, {}, {}, {}, {},assemblyDrawingBom,{secondId,secondId}),
            "Drawing accepted duplicate source body IDs");
    require(!cad::buildOrthographicDrawingSvg(assemblyDrawing,&reversedAssemblySvg,&error,nullptr,nullptr,nullptr,
            cad::OrthographicProjection::ThirdAngle,cad::DrawingSheetSize::A4Landscape,cad::DrawingAuxiliaryView::Isometric,
            0.5,{}, {}, {}, {}, {}, {}, {}, {},assemblyDrawingBom,{"missing-source-id"}),
            "Drawing accepted a source set that omitted its projected part ID");
    require(assemblySvg.contains("data-cadworkbench-source-body-ids=")&&assemblySvg.contains(id.toUtf8())&&assemblySvg.contains(secondId.toUtf8())&&
            assemblySvg.contains("X 60.000 mm"),"Multi-body drawing did not link both model bodies or their shared envelope");
    require(assemblySvg.contains("data-drawing-annotation=\"bom-balloon\"")&&
            assemblySvg.contains("data-source-body-id=\""+id.toUtf8()+"\"")&&
            assemblySvg.contains("data-bom-item-number=\""+QByteArray::number(balloonItemNumber)+"\""),
            "Drawing BOM balloon did not link its source body to the source-scoped BOM item number");
    const auto assemblySourceCheck=cad::checkDrawingSourceSnapshot(QVector<cad::Part>{*doc.find(id),*doc.find(secondId)},assemblySvg);
    require(assemblySourceCheck.errors==0&&assemblySourceCheck.warnings==0&&
            std::any_of(assemblySourceCheck.findings.cbegin(),assemblySourceCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_SOURCE_CURRENT";}),
            "Multi-body drawing source checker did not match the complete current source set");
    const auto incompleteSourceCheck=cad::checkDrawingSourceSnapshot(QVector<cad::Part>{*doc.find(id)},assemblySvg);
    require(incompleteSourceCheck.errors==1&&std::any_of(incompleteSourceCheck.findings.cbegin(),incompleteSourceCheck.findings.cend(),[](const auto& f){return f.code=="DRAWING_SOURCE_BODY_MISMATCH";}),
            "Multi-body drawing source checker accepted an incomplete selected source set");
    cad::Part changedSource=*doc.find(secondId);changedSource.geometryToken="changed-source-revision";
    const auto staleAssemblySource=cad::checkDrawingSourceSnapshot(QVector<cad::Part>{*doc.find(id),changedSource},assemblySvg);
    require(staleAssemblySource.errors==0&&staleAssemblySource.warnings==1&&
            std::any_of(staleAssemblySource.findings.cbegin(),staleAssemblySource.findings.cend(),[](const auto& f){return f.code=="DRAWING_SOURCE_GEOMETRY_STALE";}),
            "Multi-body drawing source checker failed to detect a changed component revision");
    QString bomError;
    const auto selectedBom=doc.billOfMaterials(QStringList{id,secondId},&bomError);
    require(bomError.isEmpty()&&selectedBom.size()==2&&selectedBom[0].quantity==1&&selectedBom[1].quantity==1,
            "Drawing source-scoped BOM included unrelated bodies or wrong quantities");
    const auto singleBodyBom=doc.billOfMaterials(QStringList{id},&bomError);
    require(bomError.isEmpty()&&singleBodyBom.size()==1&&singleBodyBom.first().partNumber=="PLATE-001",
            "Single-body drawing BOM did not retain the selected part metadata");
    bomError.clear();require(doc.billOfMaterials(QStringList{},&bomError).isEmpty()&&!bomError.isEmpty(),
            "Source-scoped BOM accepted an empty source selection");
    bomError.clear();require(doc.billOfMaterials(QStringList{id,"missing-body"},&bomError).isEmpty()&&!bomError.isEmpty(),
            "Source-scoped BOM accepted a missing source body");
}
void testMaterialEngineeringRecord() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Engineering material source failed");
    const QString first=doc.parts().first().id;
    require(doc.setMaterial(first,"Aluminum 6061",2700,&error),"Material assignment failed");
    require(doc.documentMaterialLibrary().size()==1 &&
            doc.documentMaterialLibrary().first().engineering.youngsModulusGPa==0.0,
            "Legacy material was not exposed with unknown modulus");
    require(!doc.setMaterialEngineering({"missing"},"Datasheet X",69,&error),
            "Engineering assignment accepted a missing body");
    require(!doc.setMaterialEngineering({first},"Datasheet\nX",69,&error) &&
            !doc.setMaterialEngineering({first},"\nDatasheet X",69,&error) &&
            !doc.setMaterialEngineering({first},"Datasheet X",-1,&error) &&
            !doc.setMaterialEngineering({first},"Datasheet X",std::numeric_limits<double>::infinity(),&error),
            "Invalid engineering values were accepted");
    require(doc.find(first)->materialEngineering.provenance.isEmpty(),
            "Invalid assignment modified engineering data");
    require(doc.setMaterialEngineering({first},"  Supplier sheet A  ",69.0,&error),
            "Engineering data assignment failed");
    require(doc.find(first)->materialEngineering.provenance=="Supplier sheet A" &&
            doc.find(first)->materialEngineering.youngsModulusGPa==69.0,
            "Engineering record was not normalized or stored");
    require(doc.undo() && doc.find(first)->materialEngineering.provenance.isEmpty(),
            "Engineering record undo failed");
    require(doc.redo() && doc.find(first)->materialEngineering.provenance=="Supplier sheet A",
            "Engineering record redo failed");
    require(doc.setMaterial(first,"Aluminum 6061",2700,&error) &&
            doc.find(first)->materialEngineering.youngsModulusGPa==69.0,
            "No-op material assignment discarded engineering metadata");
    require(doc.setPartMetadata(first,"MAT-001","Plate","A",&error),"BOM metadata setup failed");
    require(doc.duplicate(first,20,0,0,&error),"Engineering material duplication failed");
    const QString second=doc.parts().last().id;
    require(doc.find(second)->materialEngineering.provenance=="Supplier sheet A",
            "Duplicated body lost material engineering data");
    require(doc.documentMaterialLibrary().size()==1,"Document library duplicated identical entries");
    require(doc.billOfMaterials(&error).size()==1,"BOM failed to group matching engineering records");
    require(doc.setMaterialEngineering({second},"Supplier sheet B",69.0,&error),
            "Second material provenance failed");
    const auto split=doc.billOfMaterials(&error);
    require(split.size()==2 && split[0].quantity==1 && split[1].quantity==1,
            "BOM incorrectly merged different engineering provenance");
    require(doc.documentMaterialLibrary().size()==2,"Local material library omitted distinct provenance");
    require(doc.setMaterialEngineering({second},"Supplier sheet A",70.0,&error),
            "Second material modulus failed");
    require(doc.billOfMaterials(&error).size()==2,"BOM merged different Young's moduli");
    QTemporaryDir dir;require(dir.isValid(),"Engineering material temporary directory failed");
    const QString scene=dir.filePath("engineering.cadscene");
    require(doc.saveAs(scene,&error),"Engineering record scene save failed");
    const QJsonObject saved=QJsonDocument::fromJson(readBytes(scene)).object();
    const QJsonArray savedParts=saved.value("parts").toArray();
    require(savedParts.size()==2 && savedParts.at(0).toObject().value("material_engineering").isObject(),
            "Engineering JSON record not saved");
    cad::SceneDocument loaded;
    require(loaded.load(scene,&error),"Engineering record scene reload failed");
    require(loaded.find(first)->materialEngineering.provenance=="Supplier sheet A" &&
            loaded.find(second)->materialEngineering.youngsModulusGPa==70.0 &&
            loaded.documentMaterialLibrary().size()==2,
            "Engineering data lost during native scene reload");
    const QString bomCsv=dir.filePath("engineering-bom.csv");
    const QString partsCsv=dir.filePath("engineering-parts.csv");
    require(loaded.exportBillOfMaterialsCsv(bomCsv,&error) &&
            loaded.exportPartsListCsv(partsCsv,&error),"Engineering CSV export failed");
    require(readBytes(bomCsv).contains("Young's modulus (GPa)") &&
            readBytes(bomCsv).contains("Supplier sheet B")==false &&
            readBytes(bomCsv).contains("Supplier sheet A") &&
            readBytes(partsCsv).contains("Material provenance"),
            "Engineering CSV omitted metadata or contained stale provenance");
    QJsonObject invalid=saved;
    QJsonArray changed=invalid.value("parts").toArray();
    QJsonObject part=changed.at(0).toObject();
    QJsonObject engineering=part.value("material_engineering").toObject();
    engineering.insert("youngs_modulus_gpa",-10.0);
    part.insert("material_engineering",engineering);changed[0]=part;
    invalid.insert("parts",changed);
    const QString invalidPath=dir.filePath("invalid-engineering.cadscene");
    writeBytes(invalidPath,QJsonDocument(invalid).toJson());
    cad::SceneDocument rejected;
    require(!rejected.load(invalidPath,&error) && rejected.parts().isEmpty(),
            "Loader accepted invalid engineering modulus");
    // Older saved scenes do not contain the optional object.
    QJsonObject legacy=saved;QJsonArray legacyParts=legacy.value("parts").toArray();
    for(int i=0;i<legacyParts.size();++i){
        QJsonObject oldPart=legacyParts.at(i).toObject();
        oldPart.remove("material_engineering");legacyParts[i]=oldPart;
    }
    legacy.insert("parts",legacyParts);
    const QString legacyPath=dir.filePath("legacy-materials.cadscene");
    writeBytes(legacyPath,QJsonDocument(legacy).toJson());
    cad::SceneDocument legacyLoaded;
    require(legacyLoaded.load(legacyPath,&error) &&
            legacyLoaded.documentMaterialLibrary().size()==1 &&
            legacyLoaded.find(first)->materialEngineering.provenance.isEmpty() &&
            legacyLoaded.find(first)->materialEngineering.youngsModulusGPa==0.0,
            "Legacy scene did not default optional engineering data safely");
    require(loaded.setMaterial(second,"Steel",7850,&error) &&
            loaded.find(second)->materialEngineering.provenance.isEmpty() &&
            loaded.find(second)->materialEngineering.youngsModulusGPa==0.0,
            "Changing material retained stale engineering properties");
    require(loaded.setMaterial(first,"",0,&error) &&
            loaded.find(first)->materialEngineering.provenance.isEmpty(),
            "Clearing material retained engineering provenance");
}
void testBodyMaterialAssignment() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create material test body");
    const QString id=doc.parts().first().id;
    require(doc.setMaterial(id," Aluminum 6061 ",2700.0,&error),"Could not assign aluminum material");
    require(doc.find(id)->materialName=="Aluminum 6061","Material name was not normalized");
    closeTo(doc.find(id)->materialDensityKgPerM3,2700.0);
    require(doc.undo()&&doc.find(id)->materialName.isEmpty(),"Material assignment undo failed");
    require(doc.redo()&&doc.find(id)->materialName=="Aluminum 6061","Material assignment redo failed");
    cad::DensityMassProperties mass;
    require(doc.measureMassFromDensity(id,doc.find(id)->materialDensityKgPerM3,&mass,&error),"Assigned density mass query failed");
    closeTo(mass.massKg,0.004536,1e-10);
    QTemporaryDir dir;require(dir.isValid(),"Could not create material test folder");
    const QString path=dir.filePath("material.cadscene");
    require(doc.saveAs(path,&error),"Material document save failed");
    cad::SceneDocument loaded;require(loaded.load(path,&error),"Material document load failed");
    require(loaded.parts().first().materialName=="Aluminum 6061","Saved material name did not round-trip");
    closeTo(loaded.parts().first().materialDensityKgPerM3,2700.0);
    // Newline-bearing material names are invalid both at assignment and on scene load.
    require(!loaded.setMaterial(id,"Steel\nGrade",7850,&error) &&
            !loaded.setMaterial(id,"Steel\rGrade",7850,&error),
            "Multi-line material name was accepted");
    require(loaded.find(id)->materialName=="Aluminum 6061" &&
            loaded.find(id)->materialDensityKgPerM3==2700.0,
            "Rejected material name partially changed an existing assignment");
    QJsonObject invalidMaterialRoot=QJsonDocument::fromJson(readBytes(path)).object();
    QJsonArray invalidMaterialParts=invalidMaterialRoot.value("parts").toArray();
    require(!invalidMaterialParts.isEmpty(),"Missing body in invalid material scene fixture");
    QJsonObject invalidMaterialPart=invalidMaterialParts[0].toObject();
    invalidMaterialPart.insert("material","Steel\nGrade");
    invalidMaterialParts[0]=invalidMaterialPart;
    invalidMaterialRoot.insert("parts",invalidMaterialParts);
    const QString invalidMaterialPath=dir.filePath("material-newline-invalid.cadscene");
    writeBytes(invalidMaterialPath,QJsonDocument(invalidMaterialRoot).toJson());
    cad::SceneDocument invalidMaterialScene;
    require(!invalidMaterialScene.load(invalidMaterialPath,&error) &&
            invalidMaterialScene.parts().isEmpty(),
            "Scene loader accepted a multi-line material name");
    require(loaded.setMaterial(id,"Ti-6Al-4V",4430.0,&error),"Custom material assignment failed");
    require(loaded.find(id)->materialName=="Ti-6Al-4V","Custom material name was not retained");
    closeTo(loaded.find(id)->materialDensityKgPerM3,4430.0);
    require(loaded.undo()&&loaded.find(id)->materialName=="Aluminum 6061","Custom material assignment undo failed");
    require(loaded.duplicate(id,20,0,0,&error),"Material body duplicate failed");
    require(loaded.parts().last().materialName=="Aluminum 6061","Duplicate did not inherit its source material");
    closeTo(loaded.parts().last().materialDensityKgPerM3,2700.0);
    require(loaded.linearPatternAlongVector(id,0,1,0,3,20,&error),"Material body vector pattern failed");
    require(loaded.parts().last().materialName=="Aluminum 6061","Pattern instance did not inherit its source material");
    closeTo(loaded.parts().last().materialDensityKgPerM3,2700.0);
    require(loaded.mirror(id,cad::Axis::X,0,&error),"Material body mirror failed");
    require(loaded.parts().last().materialName=="Aluminum 6061","Mirror did not inherit its source material");
    closeTo(loaded.parts().last().materialDensityKgPerM3,2700.0);
    require(!loaded.setMaterial(id,"Steel",0.0,&error),"Zero-density assigned material was accepted");
    require(!loaded.setMaterial(id,"",2700.0,&error),"Density without a material name was accepted");
    require(!loaded.setMaterial(id,"Steel",std::numeric_limits<double>::infinity(),&error),"Infinite material density was accepted");
    require(!loaded.setMaterial("missing","Steel",7850,&error),"Missing body material assignment was accepted");
    require(loaded.setMaterial(id,"",0.0,&error)&&loaded.find(id)->materialName.isEmpty(),"Material clear failed");
    require(loaded.undo()&&loaded.find(id)->materialName=="Aluminum 6061","Material clear undo failed");

    cad::SceneDocument fusion;
    require(fusion.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Material fusion box setup failed");
    const QString first=fusion.parts().first().id;
    require(fusion.setMaterial(first,"Aluminum 6061",2700,&error),"Material fusion assignment failed");
    require(fusion.duplicate(first,10,0,0,&error),"Material fusion duplicate failed");
    const QString second=fusion.parts().last().id;
    require(fusion.booleanOperation(cad::BooleanOp::Fuse,first,second,&error),"Same-material union failed");
    require(fusion.parts().size()==1&&fusion.parts().first().materialName=="Aluminum 6061",
            "Union of matching materials did not retain the assignment");
    closeTo(fusion.parts().first().materialDensityKgPerM3,2700.0);
    const QString fusedId=fusion.parts().first().id;
    require(fusion.duplicate(fusedId,20,0,0,&error),"Mixed-material fusion duplicate failed");
    const QString mixedId=fusion.parts().last().id;
    require(fusion.setMaterial(mixedId,"Stainless steel 304",8000,&error),"Second union material setup failed");
    require(fusion.booleanOperation(cad::BooleanOp::Fuse,fusedId,mixedId,&error),"Mixed-material union failed");
    require(fusion.parts().size()==1&&fusion.parts().first().materialName.isEmpty(),
            "Union of different materials retained an incorrect assignment");

    cad::SceneDocument multi;
    require(multi.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"Multi-material first body setup failed");
    require(multi.addPrimitive(cad::Primitive::Sphere,{3},&error),"Multi-material second body setup failed");
    const QString firstMulti=multi.parts()[0].id,secondMulti=multi.parts()[1].id;
    require(!multi.setMaterial(QStringList{firstMulti,"missing"},"Brass",8500,&error),
            "Multi-material assignment accepted a missing body");
    require(multi.parts()[0].materialName.isEmpty()&&multi.parts()[1].materialName.isEmpty(),
            "Failed multi-material assignment partially changed bodies");
    require(multi.setMaterial(QStringList{firstMulti,secondMulti},"Brass",8500,&error),
            "Multi-body material assignment failed");
    require(multi.parts()[0].materialName=="Brass"&&multi.parts()[1].materialName=="Brass",
            "Multi-body material assignment did not update every body");
    require(multi.undo()&&multi.parts()[0].materialName.isEmpty()&&multi.parts()[1].materialName.isEmpty(),
            "One undo did not revert the full multi-body assignment");
    require(multi.redo()&&multi.parts()[0].materialName=="Brass"&&multi.parts()[1].materialName=="Brass",
            "One redo did not restore the full multi-body assignment");
}
void testPlanarSectionMeasurement() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Could not create planar-section test box");
    const QString id=doc.parts().first().id;
    const bool undoBefore=doc.canUndo();
    cad::SectionMeasurement result;
    require(doc.measurePlanarSection(id,0,20,&result,&error),"X-normal section failed");
    require(result.edgeCount==4,"X-normal box section did not produce four unique cut edges");
    closeTo(result.totalLength,100);
    require(doc.measurePlanarSection(id,2,10,&result,&error),"Z-normal section failed");
    require(result.edgeCount==4,"Z-normal box section did not produce four unique cut edges");
    closeTo(result.totalLength,140);
    require(doc.measurePlanarSection(id,1,50,&result,&error),"Nonintersecting plane query failed");
    require(result.edgeCount==0,"Plane outside the body reported section edges");closeTo(result.totalLength,0);
    require(!doc.measurePlanarSection(id,3,0,&result,&error),"Invalid section axis was accepted");
    require(!doc.measurePlanarSection(id,0,std::numeric_limits<double>::infinity(),&result,&error),"Infinite section offset was accepted");
    require(!doc.measurePlanarSection("missing",0,0,&result,&error),"Missing section body was accepted");
    require(!doc.measurePlanarSection(id,0,0,nullptr,&error),"Null section output was accepted");
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Section inspection mutated document history");
}

void testSurfaceTypeSummary() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Surface box setup failed");
    const QString boxId=doc.parts().first().id;
    const bool undoBefore=doc.canUndo();
    cad::SurfaceTypeSummary box;
    require(doc.summarizeSurfaceTypes(boxId,&box,&error),"Box surface summary failed");
    require(box.planarFaces==6&&box.cylindricalFaces==0&&box.conicalFaces==0&&
            box.sphericalFaces==0&&box.toroidalFaces==0&&box.otherFaces==0,
            "Box should consist of six planar faces");
    closeTo(box.planarArea,5200.0,1e-6);
    require(doc.addPrimitive(cad::Primitive::Cylinder,{5,12},&error),"Surface cylinder setup failed");
    const QString cylinderId=doc.parts().last().id;
    cad::SurfaceTypeSummary cylinder;
    require(doc.summarizeSurfaceTypes(cylinderId,&cylinder,&error),"Cylinder surface summary failed");
    require(cylinder.planarFaces==2&&cylinder.cylindricalFaces==1&&cylinder.otherFaces==0,
            "Cylinder should have two planar caps and one cylindrical face");
    closeTo(cylinder.planarArea,50.0*std::numbers::pi,1e-6);
    closeTo(cylinder.cylindricalArea,120.0*std::numbers::pi,1e-6);
    require(doc.addPrimitive(cad::Primitive::Sphere,{4},&error),"Surface sphere setup failed");
    cad::SurfaceTypeSummary sphere;
    require(doc.summarizeSurfaceTypes(doc.parts().last().id,&sphere,&error),"Sphere surface summary failed");
    require(sphere.sphericalFaces==1&&sphere.planarFaces==0,"Sphere surface was misclassified");
    closeTo(sphere.sphericalArea,64.0*std::numbers::pi,1e-6);
    require(doc.addPrimitive(cad::Primitive::Cone,{6,2,10},&error),"Surface cone setup failed");
    cad::SurfaceTypeSummary cone;
    require(doc.summarizeSurfaceTypes(doc.parts().last().id,&cone,&error),"Cone surface summary failed");
    require(cone.conicalFaces==1&&cone.planarFaces==2,"Cone surfaces were misclassified");
    require(doc.addPrimitive(cad::Primitive::Torus,{12,2},&error),"Surface torus setup failed");
    cad::SurfaceTypeSummary torus;
    require(doc.summarizeSurfaceTypes(doc.parts().last().id,&torus,&error),"Torus surface summary failed");
    require(torus.toroidalFaces==1&&torus.planarFaces==0,"Torus surface was misclassified");
    require(!doc.summarizeSurfaceTypes("missing",&box,&error),"Missing body was accepted");
    require(!doc.summarizeSurfaceTypes(boxId,nullptr,&error),"Null summary output was accepted");
    require(doc.parts().size()==5&&doc.canUndo()==undoBefore,"Surface summary query mutated document state");
}
void testSelectedFaceArea() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Face-area test box setup failed");
    const cad::Part part=doc.parts().first();
    const cad::FaceReference face{part.id,part.geometryToken,1};
    const bool undoBefore=doc.canUndo();
    double area=0.0;
    require(doc.measureFaceArea(face,&area,&error),"Valid face-area measurement failed");
    closeTo(area,100.0,1e-7);
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Face-area query mutated document state");
    require(!doc.measureFaceArea({part.id,part.geometryToken,0},&area,&error),"Zero face index was accepted");
    require(!doc.measureFaceArea({part.id,part.geometryToken,7},&area,&error),"Out-of-range face index was accepted");
    require(!doc.measureFaceArea({"missing",part.geometryToken,1},&area,&error),"Missing face body was accepted");
    require(!doc.measureFaceArea(face,nullptr,&error),"Null face-area output was accepted");
    require(doc.translate(part.id,5,0,0,&error),"Face-area stale-reference fixture could not move");
    require(!doc.measureFaceArea(face,&area,&error),"Stale geometry face reference was accepted");
    const cad::Part* moved=doc.find(part.id);
    require(moved&&doc.measureFaceArea({part.id,moved->geometryToken,1},&area,&error),
            "Fresh face reference failed after body transform");
    closeTo(area,100.0,1e-7);
}
void testPlanarFaceAngleMeasurement() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Face-angle test box setup failed");
    const cad::Part box=doc.parts().first();
    const bool undoBefore=doc.canUndo();
    cad::FaceAngleMeasurement measurement;
    bool foundRightAngle=false,foundOpposingFaces=false,foundParallelGap=false;int rightFirst=0,rightSecond=0;
    for(int first=1;first<=6;++first)for(int second=first+1;second<=6;++second){
        require(doc.measureAngleBetweenPlanarFaces({box.id,box.geometryToken,first},
                    {box.id,box.geometryToken,second},&measurement,&error),
                "Valid box face-angle measurement failed");
        if(std::abs(measurement.angleDegrees-90.0)<1e-7&&!foundRightAngle){
            foundRightAngle=true;rightFirst=first;rightSecond=second;
            require(!measurement.parallel,"Perpendicular faces were marked parallel");
        }
        if(std::abs(measurement.angleDegrees-180.0)<1e-7){
            foundOpposingFaces=true;
            require(measurement.parallel,"Opposing coplanar-direction faces were not marked parallel");
            if(std::abs(measurement.planeSeparationMm-10.0)<1e-7||
               std::abs(measurement.planeSeparationMm-20.0)<1e-7||
               std::abs(measurement.planeSeparationMm-30.0)<1e-7)foundParallelGap=true;
        }
        require(measurement.angleDegrees>=0.0&&measurement.angleDegrees<=180.0,
                "Face normal angle was outside [0,180]");
    }
    require(foundRightAngle&&foundOpposingFaces&&foundParallelGap,
            "Box face query missed a right angle, opposing normals, or parallel plane gap");
    require(std::abs(std::abs(measurement.firstNormalX)+std::abs(measurement.firstNormalY)+
                     std::abs(measurement.firstNormalZ)-1.0)<1e-7,
            "Returned face normal was not unit length");
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Face-angle query mutated document state");
    require(!doc.measureAngleBetweenPlanarFaces({box.id,box.geometryToken,0},
                {box.id,box.geometryToken,1},&measurement,&error),"Zero face index was accepted");
    require(!doc.measureAngleBetweenPlanarFaces({box.id,box.geometryToken,1},
                {box.id,box.geometryToken,7},&measurement,&error),"Out-of-range face index was accepted");
    require(!doc.measureAngleBetweenPlanarFaces({"missing",box.geometryToken,1},
                {box.id,box.geometryToken,2},&measurement,&error),"Missing face body was accepted");
    require(!doc.measureAngleBetweenPlanarFaces({box.id,box.geometryToken,1},
                {box.id,box.geometryToken,2},nullptr,&error),"Null face-angle output was accepted");
    require(doc.translate(box.id,5,0,0,&error),"Face-angle stale-reference fixture could not move");
    require(!doc.measureAngleBetweenPlanarFaces({box.id,box.geometryToken,1},
                {box.id,box.geometryToken,2},&measurement,&error),"Stale face reference was accepted");
    const cad::Part* moved=doc.find(box.id);
    require(moved&&doc.measureAngleBetweenPlanarFaces({box.id,moved->geometryToken,rightFirst},
                {box.id,moved->geometryToken,rightSecond},&measurement,&error),
            "Fresh face references failed after body transform");
    closeTo(measurement.angleDegrees,90.0);
    require(!measurement.parallel,"Fresh adjacent faces should not be parallel");

    cad::SceneDocument curved;
    require(curved.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Curved-face fixture box setup failed");
    const cad::Part planar=curved.parts().first();
    require(curved.addPrimitive(cad::Primitive::Cylinder,{5,10},&error),"Curved-face cylinder setup failed");
    const cad::Part cylinder=curved.parts().last();
    bool rejectedCurved=false;
    for(int face=1;face<=3&&!rejectedCurved;++face)
        rejectedCurved=!curved.measureAngleBetweenPlanarFaces({planar.id,planar.geometryToken,1},
            {cylinder.id,cylinder.geometryToken,face},&measurement,&error);
    require(rejectedCurved&&error.contains("planar faces"),"Non-planar face was not rejected with a clear error");
}
void testCylindricalFaceMeasurement() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Cylinder,{5.0,20.0},&error),"Cylindrical-face test cylinder setup failed");
    const cad::Part cylinder=doc.parts().first();
    const bool undoBefore=doc.canUndo();
    cad::CylindricalFaceMeasurement measurement;bool foundCylinder=false;int cylinderFace=0,nonCylinderFaces=0;
    for(int face=1;face<=3;++face){
        cad::CylindricalFaceMeasurement candidate;
        if(doc.measureCylindricalFace({cylinder.id,cylinder.geometryToken,face},&candidate,&error)){
            measurement=candidate;foundCylinder=true;cylinderFace=face;
            closeTo(candidate.radiusMm,5.0);closeTo(candidate.diameterMm,10.0);
            closeTo(candidate.axisX*candidate.axisX+candidate.axisY*candidate.axisY+candidate.axisZ*candidate.axisZ,1.0);
        }else{
            ++nonCylinderFaces;
            require(error.contains("cylindrical side"),"Planar cap rejection message was unclear");
        }
    }
    require(foundCylinder&&cylinderFace>0&&nonCylinderFaces==2,
            "Cylinder side face or planar cap classification was incorrect");
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Cylindrical-face query mutated document state");
    require(!doc.measureCylindricalFace({cylinder.id,cylinder.geometryToken,0},&measurement,&error),
            "Zero cylindrical face index was accepted");
    require(!doc.measureCylindricalFace({cylinder.id,cylinder.geometryToken,4},&measurement,&error),
            "Out-of-range cylindrical face index was accepted");
    require(!doc.measureCylindricalFace({"missing",cylinder.geometryToken,cylinderFace},&measurement,&error),
            "Missing cylindrical face body was accepted");
    require(!doc.measureCylindricalFace({cylinder.id,cylinder.geometryToken,cylinderFace},nullptr,&error),
            "Null cylindrical-face output was accepted");
    require(doc.translate(cylinder.id,0,3,0,&error),"Cylindrical-face stale-reference fixture could not move");
    require(!doc.measureCylindricalFace({cylinder.id,cylinder.geometryToken,cylinderFace},&measurement,&error),
            "Stale cylindrical face reference was accepted");
    const cad::Part* moved=doc.find(cylinder.id);
    require(moved&&doc.measureCylindricalFace({cylinder.id,moved->geometryToken,cylinderFace},&measurement,&error),
            "Fresh cylindrical face reference failed after transform");
    closeTo(measurement.radiusMm,5.0);closeTo(measurement.diameterMm,10.0);

    cad::SceneDocument box;
    require(box.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Planar-face rejection fixture setup failed");
    const cad::Part planar=box.parts().first();
    require(!box.measureCylindricalFace({planar.id,planar.geometryToken,1},&measurement,&error),
            "Planar face was accepted as cylindrical");
}
void testSelectedEdgeLength() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Edge-length test box setup failed");
    const cad::Part part=doc.parts().first();
    const cad::EdgeReference edge{part.id,part.geometryToken,1};
    const bool undoBefore=doc.canUndo();
    double length=0.0;
    double first=0.0,second=0.0,ninth=0.0;int measured=0;
    for(int index=1;index<=24;++index){
        require(doc.measureEdgeLength({part.id,part.geometryToken,index},&length,&error),"Valid edge-length measurement failed");
        ++measured;
        if(index==1)first=length;
        if(index==2)second=length;
        if(index==9)ninth=length;
    }
    require(measured==24,"The box edge traversal was incomplete");
    closeTo(first,30.0,1e-7);closeTo(second,20.0,1e-7);closeTo(ninth,10.0,1e-7);
    require(doc.parts().size()==1&&doc.canUndo()==undoBefore,"Edge-length query mutated document state");
    require(!doc.measureEdgeLength({part.id,part.geometryToken,0},&length,&error),"Zero edge index was accepted");
    require(!doc.measureEdgeLength({part.id,part.geometryToken,99999},&length,&error),"Out-of-range edge index was accepted");
    require(!doc.measureEdgeLength({"missing",part.geometryToken,1},&length,&error),"Missing body was accepted");
    require(!doc.measureEdgeLength(edge,nullptr,&error),"Null edge-length output was accepted");
    require(doc.translate(part.id,5,0,0,&error),"Edge-length stale-reference fixture could not move");
    require(!doc.measureEdgeLength(edge,&length,&error),"Stale geometry edge reference was accepted");
    const cad::Part* moved=doc.find(part.id);
    require(moved&&doc.measureEdgeLength({part.id,moved->geometryToken,1},&length,&error),
            "Fresh edge reference failed after body transform");
    closeTo(length,30.0,1e-7);
}
void testSelectedVertexCoordinates() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Vertex-coordinate test box setup failed");
    const cad::Part part=doc.parts().first();
    bool sawOrigin=false,sawOpposite=false;
    double x=0,y=0,z=0;
    int measured=0;
    for(int index=1;index<=100;++index){
        if(!doc.measureVertexCoordinates({part.id,part.geometryToken,index},&x,&y,&z,&error))break;
        ++measured;
        require(x>=-1e-8&&x<=10+1e-8&&y>=-1e-8&&y<=20+1e-8&&z>=-1e-8&&z<=30+1e-8,
                "Vertex coordinate lies outside the box bounds");
        sawOrigin|=std::abs(x)<1e-8&&std::abs(y)<1e-8&&std::abs(z)<1e-8;
        sawOpposite|=std::abs(x-10)<1e-8&&std::abs(y-20)<1e-8&&std::abs(z-30)<1e-8;
    }
    require(measured>=8&&sawOrigin&&sawOpposite,"Box corner coordinates did not match its bounds");
    require(!doc.measureVertexCoordinates({part.id,part.geometryToken,0},&x,&y,&z,&error),"Zero vertex index was accepted");
    require(!doc.measureVertexCoordinates({part.id,part.geometryToken,99999},&x,&y,&z,&error),"Out-of-range vertex index was accepted");
    require(!doc.measureVertexCoordinates({"missing",part.geometryToken,1},&x,&y,&z,&error),"Missing vertex body was accepted");
    require(!doc.measureVertexCoordinates({part.id,part.geometryToken,1},nullptr,&y,&z,&error),"Null vertex coordinate output was accepted");
    require(doc.translate(part.id,5,0,0,&error),"Vertex stale-reference fixture could not move");
    require(!doc.measureVertexCoordinates({part.id,part.geometryToken,1},&x,&y,&z,&error),"Stale vertex reference was accepted");
    const cad::Part* moved=doc.find(part.id);
    require(moved&&doc.measureVertexCoordinates({part.id,moved->geometryToken,1},&x,&y,&z,&error),"Fresh vertex reference failed after transform");
}
void testVertexDistance() {
    cad::SceneDocument doc;QString error;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Vertex-distance test box setup failed");
    const cad::Part part=doc.parts().first();
    cad::VertexReference origin,opposite;double x=0,y=0,z=0;
    for(int index=1;index<=100;++index){
        if(!doc.measureVertexCoordinates({part.id,part.geometryToken,index},&x,&y,&z,&error))break;
        if(std::abs(x)<1e-8&&std::abs(y)<1e-8&&std::abs(z)<1e-8)origin={part.id,part.geometryToken,index};
        if(std::abs(x-10)<1e-8&&std::abs(y-20)<1e-8&&std::abs(z-30)<1e-8)opposite={part.id,part.geometryToken,index};
    }
    require(origin.vertexIndex>0&&opposite.vertexIndex>0,"Could not find opposite box vertices");
    double distance=0;
    require(doc.measureVertexDistance(origin,opposite,&distance,&error),"Valid vertex distance failed");
    closeTo(distance,std::sqrt(1400.0),1e-8);
    double zeroDistance=1.0;
    require(doc.measureVertexDistance(origin,origin,&zeroDistance,&error),"Coincident vertex distance failed");
    closeTo(zeroDistance,0.0,1e-12);
    double reverseDistance=0.0;
    require(doc.measureVertexDistance(opposite,origin,&reverseDistance,&error),"Reversed vertex distance failed");
    closeTo(reverseDistance,distance,1e-12);
    require(!doc.measureVertexDistance(origin,opposite,nullptr,&error),"Null vertex-distance output was accepted");
    require(!doc.measureVertexDistance({"missing",part.geometryToken,origin.vertexIndex},opposite,&distance,&error),"Missing vertex body was accepted");
    require(doc.translate(part.id,5,0,0,&error),"Vertex-distance stale-reference fixture could not move");
    require(!doc.measureVertexDistance(origin,opposite,&distance,&error),"Stale vertex reference was accepted for distance");
}
void testCircularHolePattern() {
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Could not create circular-hole-pattern test body");
    const QString id=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    require(!doc.circularHolePattern(id,40,40,20,25,6,20,true,1,360,&error)&&doc.parts().size()==1,
            "Circular hole pattern accepted fewer than two holes");
    require(!doc.circularHolePattern(id,40,40,20,0,6,20,true,4,360,&error)&&doc.parts().size()==1,
            "Circular hole pattern accepted a zero bolt-circle radius");
    require(!doc.circularHolePattern(id,40,40,20,25,6,20,true,4,0,&error)&&doc.parts().size()==1,
            "Circular hole pattern accepted a zero angle");
    require(!doc.circularHolePattern(id,500,500,20,25,6,20,true,4,360,&error)&&doc.parts().size()==1,
            "Circular hole pattern accepted cutters missing the target body");
    closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.circularHolePattern(id,40,40,20,25,6,20,true,4,360,&error),
            ("Four-hole circular pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());
    closeTo(volume(doc.parts().first().shape),originalVolume-4*std::numbers::pi*9*20,1e-4);
    require(doc.undo()&&doc.parts().size()==1,"Circular hole pattern undo failed");
    closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.redo()&&doc.parts().size()==1,"Circular hole pattern redo failed");
    validate(doc.parts().first());
}
void testCircularCountersinkPattern(){
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Countersink-pattern body creation failed");
    const QString id=doc.parts().first().id;const double original=volume(doc.parts().first().shape);
    require(!doc.circularCountersinkPattern(id,40,40,20,25,6,20,true,5,90,4,360,&error),"Countersink pattern accepted a head narrower than the drill");
    require(!doc.circularCountersinkPattern(id,40,40,20,25,6,20,true,10,180,4,360,&error),"Countersink pattern accepted a boundary angle");
    require(!doc.circularCountersinkPattern(id,40,40,20,25,6,20,true,10,90,1,360,&error),"Countersink pattern accepted a single hole");
    require(!doc.circularCountersinkPattern(id,40,40,20,0,6,20,true,10,90,4,360,&error),"Countersink pattern accepted zero bolt-circle radius");
    require(!doc.circularCountersinkPattern(id,400,400,20,25,6,20,true,10,90,4,360,&error),"Countersink pattern accepted cutters outside the body");
    require(!doc.circularCountersinkPattern("missing",40,40,20,25,6,20,true,10,90,4,360,&error),"Countersink pattern accepted an unknown body");
    closeTo(volume(doc.parts().first().shape),original);
    require(doc.circularCountersinkPattern(id,40,40,20,25,6,20,true,10,90,4,360,&error),
            ("Circular countersink pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());const double cut=original-volume(doc.parts().first().shape);
    require(cut>2400&&cut<2800,"Countersink pattern removed an unexpected volume");
    require(doc.undo(),"Countersink pattern undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Countersink pattern redo failed");closeTo(volume(doc.parts().first().shape),original-cut);

    cad::SceneDocument blind;
    require(blind.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Blind countersink pattern fixture failed");
    require(blind.circularCountersinkPattern(blind.parts().first().id,40,40,20,25,6,8,false,10,82,3,180,&error),
            ("Blind partial countersink pattern failed: "+error.toStdString()).c_str());
    validate(blind.parts().first());const double blindPatternVolume=volume(blind.parts().first().shape);
    require(blindPatternVolume<128000,"Blind partial countersinks removed no material");
    require(blind.undo(),"Blind partial countersink pattern undo failed");closeTo(volume(blind.parts().first().shape),128000);
    require(blind.redo(),"Blind partial countersink pattern redo failed");closeTo(volume(blind.parts().first().shape),blindPatternVolume);

    cad::SceneDocument lateFailure;
    require(lateFailure.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Late-failure countersink fixture failed");
    const QString lateFailureId=lateFailure.parts().first().id;
    require(!lateFailure.circularCountersinkPattern(lateFailureId,20,40,20,30,6,20,true,10,90,2,180,&error),
            "Countersink pattern accepted a later cutter missing the body");
    closeTo(volume(lateFailure.parts().first().shape),128000);
}
void testCircularCounterborePattern(){
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Counterbore-pattern body creation failed");
    const QString id=doc.parts().first().id;const double original=volume(doc.parts().first().shape);
    require(!doc.circularCounterborePattern(id,40,40,20,25,6,20,true,5,3,4,360,&error),"Counterbore pattern accepted a head narrower than the drill");
    require(!doc.circularCounterborePattern(id,40,40,20,25,6,8,false,10,9,4,360,&error),"Counterbore pattern accepted head depth beyond blind depth");
    require(!doc.circularCounterborePattern(id,40,40,20,25,6,20,true,10,3,1,360,&error),"Counterbore pattern accepted a single hole");
    require(!doc.circularCounterborePattern(id,40,40,20,0,6,20,true,10,3,4,360,&error),"Counterbore pattern accepted zero bolt-circle radius");
    require(!doc.circularCounterborePattern(id,400,400,20,25,6,20,true,10,3,4,360,&error),"Counterbore pattern accepted cutters outside the body");
    require(!doc.circularCounterborePattern("missing",40,40,20,25,6,20,true,10,3,4,360,&error),"Counterbore pattern accepted an unknown body");
    closeTo(volume(doc.parts().first().shape),original);
    require(doc.circularCounterborePattern(id,40,40,20,25,6,20,true,10,3,4,360,&error),
            ("Circular counterbore pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());const double cut=original-volume(doc.parts().first().shape);
    require(cut>2700&&cut<3000,"Counterbore pattern removed an unexpected volume");
    require(doc.undo(),"Counterbore pattern undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Counterbore pattern redo failed");closeTo(volume(doc.parts().first().shape),original-cut);
    cad::SceneDocument blind;require(blind.addPrimitive(cad::Primitive::Box,{80,80,20},&error),"Blind counterbore fixture failed");
    require(blind.circularCounterborePattern(blind.parts().first().id,40,40,20,25,6,8,false,10,3,3,180,&error),
            ("Blind partial counterbore pattern failed: "+error.toStdString()).c_str());
    validate(blind.parts().first());require(volume(blind.parts().first().shape)<128000,"Blind counterbores removed no material");
}
void testLinearKeyholeSlotPattern(){
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Keyhole-pattern box fixture failed");
    const QString id=doc.parts().first().id;const double original=volume(doc.parts().first().shape);
    require(!doc.linearKeyholeSlotPattern(id,8,15,20,8,8,10,6,0,3,12,0,&error),"Keyhole pattern accepted a throat as wide as the head");
    require(!doc.linearKeyholeSlotPattern(id,8,15,20,8,4,10,6,361,3,12,0,&error),"Keyhole pattern accepted invalid rotation");
    require(!doc.linearKeyholeSlotPattern(id,8,15,20,8,4,10,6,0,1,12,0,&error),"Keyhole pattern accepted one instance");
    require(!doc.linearKeyholeSlotPattern(id,8,15,20,8,4,10,6,0,3,0,0,&error),"Keyhole pattern accepted zero spacing");
    require(!doc.linearKeyholeSlotPattern("missing",8,15,20,8,4,10,6,0,3,12,0,&error),"Keyhole pattern accepted an unknown body");
    closeTo(volume(doc.parts().first().shape),original);
    require(doc.linearKeyholeSlotPattern(id,8,15,20,8,4,10,6,0,3,12,0,&error),
            ("Linear keyhole-slot pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());const double cut=original-volume(doc.parts().first().shape);
    require(cut>1000&&cut<2000,"Keyhole pattern removed an unexpected volume");
    require(doc.undo(),"Keyhole-slot pattern undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Keyhole-slot pattern redo failed");closeTo(volume(doc.parts().first().shape),original-cut);
    cad::SceneDocument lateFailure;require(lateFailure.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Late-failure keyhole fixture failed");
    const double lateFailureVolume=volume(lateFailure.parts().first().shape);
    require(!lateFailure.linearKeyholeSlotPattern(lateFailure.parts().first().id,8,15,20,8,4,10,6,0,2,-20,0,&error),
            "Keyhole pattern accepted a later slot missing the body");
    closeTo(volume(lateFailure.parts().first().shape),lateFailureVolume);
    cad::SceneDocument rotated;require(rotated.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated keyhole pattern fixture failed");
    require(rotated.linearKeyholeSlotPattern(rotated.parts().first().id,20,9,20,6,3,8,5,90,2,0,12,&error),
            ("Rotated keyhole-slot pattern failed: "+error.toStdString()).c_str());
    validate(rotated.parts().first());
}
void testLinearCounterborePattern(){
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Linear counterbore fixture failed");
    const QString id=doc.parts().first().id;const double original=volume(doc.parts().first().shape);
    require(!doc.linearCounterborePattern(id,8,15,20,4,20,true,4,2,3,12,0,&error),"Linear counterbore accepted a head no wider than its drill");
    require(!doc.linearCounterborePattern(id,8,15,20,4,6,false,8,7,3,12,0,&error),"Linear counterbore accepted a seat deeper than the blind hole");
    require(!doc.linearCounterborePattern(id,8,15,20,4,20,true,8,2,1,12,0,&error),"Linear counterbore accepted one instance");
    require(!doc.linearCounterborePattern(id,8,15,20,4,20,true,8,2,3,0,0,&error),"Linear counterbore accepted zero spacing");
    require(!doc.linearCounterborePattern("missing",8,15,20,4,20,true,8,2,3,12,0,&error),"Linear counterbore accepted a missing body");
    closeTo(volume(doc.parts().first().shape),original);
    require(doc.linearCounterborePattern(id,8,15,20,4,20,true,8,2,3,12,0,&error),
            ("Linear counterbore pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());const double cut=original-volume(doc.parts().first().shape);
    require(cut>900&&cut<1050,"Linear counterbore pattern removed unexpected volume");
    require(doc.undo(),"Linear counterbore undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Linear counterbore redo failed");closeTo(volume(doc.parts().first().shape),original-cut);
    cad::SceneDocument blind;require(blind.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Blind counterbore fixture failed");
    require(blind.linearCounterborePattern(blind.parts().first().id,10,15,20,4,8,false,8,2,2,20,0,&error),
            ("Blind linear counterbore pattern failed: "+error.toStdString()).c_str());
    validate(blind.parts().first());require(volume(blind.parts().first().shape)<24000,"Blind counterbores removed no material");
    cad::SceneDocument rollback;require(rollback.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rollback counterbore fixture failed");
    const double rollbackVolume=volume(rollback.parts().first().shape);
    require(!rollback.linearCounterborePattern(rollback.parts().first().id,8,15,20,4,20,true,8,2,2,-20,0,&error),
            "Linear counterbore accepted a later cutter missing the body");
    closeTo(volume(rollback.parts().first().shape),rollbackVolume);
}
void testLinearCountersinkPattern(){
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Linear countersink fixture failed");
    const QString id=doc.parts().first().id;const double original=volume(doc.parts().first().shape);
    require(!doc.linearCountersinkPattern(id,8,15,20,4,20,true,4,90,3,12,0,&error),"Linear countersink accepted a head no wider than its drill");
    require(!doc.linearCountersinkPattern(id,8,15,20,4,20,true,8,1,3,12,0,&error),"Linear countersink accepted an invalid included angle");
    require(!doc.linearCountersinkPattern(id,8,15,20,4,20,true,8,90,1,12,0,&error),"Linear countersink accepted one instance");
    require(!doc.linearCountersinkPattern(id,8,15,20,4,20,true,8,90,3,0,0,&error),"Linear countersink accepted zero spacing");
    require(!doc.linearCountersinkPattern("missing",8,15,20,4,20,true,8,90,3,12,0,&error),"Linear countersink accepted a missing body");
    closeTo(volume(doc.parts().first().shape),original);
    require(doc.linearCountersinkPattern(id,8,15,20,4,20,true,8,90,3,12,0,&error),
            ("Linear countersink pattern failed: "+error.toStdString()).c_str());
    validate(doc.parts().first());const double cut=original-volume(doc.parts().first().shape);
    require(cut>850&&cut<1000,"Linear countersink pattern removed unexpected volume");
    require(doc.undo(),"Linear countersink undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Linear countersink redo failed");closeTo(volume(doc.parts().first().shape),original-cut);
    cad::SceneDocument blind;require(blind.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Blind countersink fixture failed");
    require(blind.linearCountersinkPattern(blind.parts().first().id,10,15,20,4,8,false,8,82,2,20,0,&error),
            ("Blind linear countersink pattern failed: "+error.toStdString()).c_str());
    validate(blind.parts().first());require(volume(blind.parts().first().shape)<24000,"Blind linear countersinks removed no material");
    cad::SceneDocument rollback;require(rollback.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rollback countersink fixture failed");
    const double rollbackVolume=volume(rollback.parts().first().shape);
    require(!rollback.linearCountersinkPattern(rollback.parts().first().id,8,15,20,4,20,true,8,90,2,-20,0,&error),
            "Linear countersink accepted a later cutter missing the body");
    closeTo(volume(rollback.parts().first().shape),rollbackVolume);
}
void testDovetailGroove() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Dovetail source box creation failed");
    const QString sourceId=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    require(std::abs(originalVolume-24000.0)<1e-5,"Dovetail fixture volume is wrong");

    cad::DovetailGrooveCommand command;
    require(!doc.prepareDovetailGroove(sourceId,20,15,20,8,8,5,18,0,&command,&error),
            "Dovetail accepted an undercut width equal to its mouth");
    require(!doc.prepareDovetailGroove(sourceId,20,15,20,8,14,5,18,361,&command,&error),
            "Dovetail accepted a rotation outside its supported range");
    require(!doc.prepareDovetailGroove("missing",20,15,20,8,14,5,18,0,&command,&error),
            "Dovetail accepted an unknown target body");
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Rejected dovetail inputs changed the target body");

    require(doc.prepareDovetailGroove(sourceId,20,15,20,8,14,5,18,0,&command,&error),
            ("Dovetail preparation failed: "+error.toStdString()).c_str());
    const auto preview=cad::SceneDocument::buildDovetailGroovePreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Dovetail preview was invalid: "+preview.error.toStdString()).c_str());
    require(std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Building a dovetail preview mutated the source body");
    const double removed=originalVolume-volume(preview.shape);
    require(removed>980&&removed<1005,"Dovetail removed an unexpected amount of material");
    require(doc.commitDovetailGroove(command,preview.shape,&error),
            ("Dovetail commit failed: "+error.toStdString()).c_str());
    closeTo(volume(doc.parts().first().shape),volume(preview.shape));
    require(!doc.commitDovetailGroove(command,preview.shape,&error),
            "Dovetail preview was reusable after its source geometry changed");
    require(doc.undo(),"Dovetail undo failed");
    closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.redo(),"Dovetail redo failed");
    closeTo(volume(doc.parts().first().shape),originalVolume-removed);

    cad::SceneDocument rotatedDoc;
    require(rotatedDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated dovetail fixture failed");
    cad::DovetailGrooveCommand rotatedCommand;
    require(rotatedDoc.prepareDovetailGroove(rotatedDoc.parts().first().id,20,15,20,6,12,4,12,30,
                                              &rotatedCommand,&error),"Rotated dovetail preparation failed");
    const auto rotatedPreview=cad::SceneDocument::buildDovetailGroovePreview(rotatedCommand);
    require(!rotatedPreview.shape.IsNull()&&BRepCheck_Analyzer(rotatedPreview.shape).IsValid(),
            ("Rotated dovetail preview invalid: "+rotatedPreview.error.toStdString()).c_str());
    require(volume(rotatedPreview.shape)<volume(rotatedDoc.parts().first().shape),
            "Rotated dovetail did not remove material");

    cad::DovetailGrooveCommand missedCommand;
    require(rotatedDoc.prepareDovetailGroove(rotatedDoc.parts().first().id,200,200,20,6,12,4,12,0,
                                              &missedCommand,&error),"Missed dovetail preparation failed early");
    require(cad::SceneDocument::buildDovetailGroovePreview(missedCommand).shape.IsNull(),
            "Dovetail outside the body produced a preview");

    cad::SceneDocument staleDoc;
    require(staleDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Stale dovetail fixture failed");
    cad::DovetailGrooveCommand staleCommand;
    require(staleDoc.prepareDovetailGroove(staleDoc.parts().first().id,20,15,20,8,14,5,18,0,
                                            &staleCommand,&error),"Stale dovetail preparation failed");
    const auto stalePreview=cad::SceneDocument::buildDovetailGroovePreview(staleCommand);
    require(!stalePreview.shape.IsNull(),"Stale dovetail fixture preview failed");
    require(staleDoc.translate(staleDoc.parts().first().id,1,0,0,&error),"Stale dovetail mutation failed");
    require(!staleDoc.commitDovetailGroove(staleCommand,stalePreview.shape,&error),
            "Stale dovetail preview committed after target geometry changed");
}
void testTSlotGroove(){
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"T-slot source box creation failed");
    const QString sourceId=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    cad::TSlotGrooveCommand command;
    require(!doc.prepareTSlotGroove(sourceId,20,15,20,8,8,6,2,18,0,&command,&error),
            "T-slot accepted an undercut width equal to its neck");
    require(!doc.prepareTSlotGroove(sourceId,20,15,20,6,12,6,6,18,0,&command,&error),
            "T-slot accepted an undercut height equal to total depth");
    require(!doc.prepareTSlotGroove(sourceId,20,15,20,6,12,6,2,18,-361,&command,&error),
            "T-slot accepted an out-of-range rotation");
    require(!doc.prepareTSlotGroove("missing",20,15,20,6,12,6,2,18,0,&command,&error),
            "T-slot accepted an unknown target body");
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Rejected T-slot dimensions modified the body");

    require(doc.prepareTSlotGroove(sourceId,20,15,20,6,12,6,2,18,0,&command,&error),
            ("T-slot preparation failed: "+error.toStdString()).c_str());
    const auto preview=cad::SceneDocument::buildTSlotGroovePreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("T-slot preview invalid: "+preview.error.toStdString()).c_str());
    require(std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "T-slot preview changed its source before commit");
    const double removed=originalVolume-volume(preview.shape);
    require(removed>700&&removed<1000,"T-slot removed an unexpected amount of material");
    require(doc.commitTSlotGroove(command,preview.shape,&error),
            ("T-slot commit failed: "+error.toStdString()).c_str());
    closeTo(volume(doc.parts().first().shape),volume(preview.shape));
    require(!doc.commitTSlotGroove(command,preview.shape,&error),"Committed T-slot preview was reusable");
    require(doc.undo(),"T-slot undo failed");closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.redo(),"T-slot redo failed");closeTo(volume(doc.parts().first().shape),originalVolume-removed);

    cad::SceneDocument rotatedDoc;
    require(rotatedDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated T-slot fixture failed");
    cad::TSlotGrooveCommand rotatedCommand;
    require(rotatedDoc.prepareTSlotGroove(rotatedDoc.parts().first().id,20,15,20,5,10,5,1.5,12,45,
                                           &rotatedCommand,&error),"Rotated T-slot preparation failed");
    const auto rotatedPreview=cad::SceneDocument::buildTSlotGroovePreview(rotatedCommand);
    require(!rotatedPreview.shape.IsNull()&&BRepCheck_Analyzer(rotatedPreview.shape).IsValid(),
            ("Rotated T-slot preview invalid: "+rotatedPreview.error.toStdString()).c_str());
    require(volume(rotatedPreview.shape)<volume(rotatedDoc.parts().first().shape),
            "Rotated T-slot failed to remove material");

    cad::TSlotGrooveCommand missedCommand;
    require(rotatedDoc.prepareTSlotGroove(rotatedDoc.parts().first().id,200,200,20,5,10,5,1.5,12,0,
                                           &missedCommand,&error),"Missed T-slot preparation failed early");
    require(cad::SceneDocument::buildTSlotGroovePreview(missedCommand).shape.IsNull(),
            "T-slot cutter outside the body produced a preview");

    cad::SceneDocument staleDoc;
    require(staleDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Stale T-slot fixture failed");
    cad::TSlotGrooveCommand staleCommand;
    require(staleDoc.prepareTSlotGroove(staleDoc.parts().first().id,20,15,20,6,12,6,2,18,0,
                                         &staleCommand,&error),"Stale T-slot preparation failed");
    const auto stalePreview=cad::SceneDocument::buildTSlotGroovePreview(staleCommand);
    require(!stalePreview.shape.IsNull(),"Stale T-slot fixture preview failed");
    require(staleDoc.translate(staleDoc.parts().first().id,1,0,0,&error),"Stale T-slot mutation failed");
    require(!staleDoc.commitTSlotGroove(staleCommand,stalePreview.shape,&error),
            "Stale T-slot preview committed after the body changed");
}
void testRoundedSlot(){
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rounded-slot source box creation failed");
    const QString sourceId=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    cad::RoundedSlotCommand command;
    require(!doc.prepareRoundedSlot(sourceId,20,15,20,0,18,6,0,&command,&error),
            "Rounded slot accepted zero width");
    require(!doc.prepareRoundedSlot(sourceId,20,15,20,8,7,6,0,&command,&error),
            "Rounded slot accepted overall length shorter than width");
    require(!doc.prepareRoundedSlot(sourceId,20,15,20,8,18,6,361,&command,&error),
            "Rounded slot accepted out-of-range rotation");
    require(!doc.prepareRoundedSlot("missing",20,15,20,8,18,6,0,&command,&error),
            "Rounded slot accepted an unknown target body");
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Rejected rounded-slot dimensions modified the source");

    require(doc.prepareRoundedSlot(sourceId,20,15,20,8,18,6,0,&command,&error),
            ("Rounded-slot preparation failed: "+error.toStdString()).c_str());
    const auto preview=cad::SceneDocument::buildRoundedSlotPreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Rounded-slot preview invalid: "+preview.error.toStdString()).c_str());
    require(std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Rounded-slot preview changed its source before commit");
    const double removed=originalVolume-volume(preview.shape);
    require(removed>700&&removed<900,"Rounded slot removed an unexpected amount of material");
    require(doc.commitRoundedSlot(command,preview.shape,&error),
            ("Rounded-slot commit failed: "+error.toStdString()).c_str());
    closeTo(volume(doc.parts().first().shape),volume(preview.shape));
    require(!doc.commitRoundedSlot(command,preview.shape,&error),"Committed rounded-slot preview was reusable");
    require(doc.undo(),"Rounded-slot undo failed");closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.redo(),"Rounded-slot redo failed");closeTo(volume(doc.parts().first().shape),originalVolume-removed);

    cad::SceneDocument circularDoc;
    require(circularDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Circular-slot fixture creation failed");
    cad::RoundedSlotCommand circularCommand;
    require(circularDoc.prepareRoundedSlot(circularDoc.parts().first().id,20,15,20,8,8,5,90,
                                            &circularCommand,&error),"Circular rounded-slot preparation failed");
    const auto circularPreview=cad::SceneDocument::buildRoundedSlotPreview(circularCommand);
    require(!circularPreview.shape.IsNull()&&BRepCheck_Analyzer(circularPreview.shape).IsValid(),
            ("Length-equals-width slot failed: "+circularPreview.error.toStdString()).c_str());
    require(volume(circularPreview.shape)<volume(circularDoc.parts().first().shape),
            "Circular degenerate slot did not remove material");

    cad::SceneDocument rotatedDoc;
    require(rotatedDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated rounded-slot fixture failed");
    cad::RoundedSlotCommand rotatedCommand;
    require(rotatedDoc.prepareRoundedSlot(rotatedDoc.parts().first().id,20,15,20,6,16,5,45,
                                           &rotatedCommand,&error),"Rotated rounded-slot preparation failed");
    const auto rotatedPreview=cad::SceneDocument::buildRoundedSlotPreview(rotatedCommand);
    require(!rotatedPreview.shape.IsNull()&&BRepCheck_Analyzer(rotatedPreview.shape).IsValid(),
            ("Rotated rounded-slot preview invalid: "+rotatedPreview.error.toStdString()).c_str());

    cad::RoundedSlotCommand missedCommand;
    require(rotatedDoc.prepareRoundedSlot(rotatedDoc.parts().first().id,200,200,20,6,16,5,0,
                                           &missedCommand,&error),"Missed rounded-slot preparation failed early");
    require(cad::SceneDocument::buildRoundedSlotPreview(missedCommand).shape.IsNull(),
            "Rounded slot outside the body produced a preview");

    cad::SceneDocument staleDoc;
    require(staleDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Stale rounded-slot fixture failed");
    cad::RoundedSlotCommand staleCommand;
    require(staleDoc.prepareRoundedSlot(staleDoc.parts().first().id,20,15,20,8,18,6,0,
                                         &staleCommand,&error),"Stale rounded-slot preparation failed");
    const auto stalePreview=cad::SceneDocument::buildRoundedSlotPreview(staleCommand);
    require(!stalePreview.shape.IsNull(),"Stale rounded-slot fixture preview failed");
    require(staleDoc.translate(staleDoc.parts().first().id,1,0,0,&error),"Stale rounded-slot mutation failed");
    require(!staleDoc.commitRoundedSlot(staleCommand,stalePreview.shape,&error),
            "Stale rounded-slot preview committed after the body changed");
}
void testKeyholeSlot(){
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Keyhole source box creation failed");
    const QString sourceId=doc.parts().first().id;
    const double originalVolume=volume(doc.parts().first().shape);
    cad::KeyholeSlotCommand command;
    require(!doc.prepareKeyholeSlot(sourceId,20,10,20,8,8,10,6,0,&command,&error),
            "Keyhole slot accepted throat width equal to head diameter");
    require(!doc.prepareKeyholeSlot(sourceId,20,10,20,8,4,0,6,0,&command,&error),
            "Keyhole slot accepted zero throat length");
    require(!doc.prepareKeyholeSlot(sourceId,20,10,20,8,4,10,6,-361,&command,&error),
            "Keyhole slot accepted out-of-range rotation");
    require(!doc.prepareKeyholeSlot("missing",20,10,20,8,4,10,6,0,&command,&error),
            "Keyhole slot accepted an unknown target body");
    require(doc.parts().size()==1&&std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Rejected keyhole-slot inputs modified the source");

    require(doc.prepareKeyholeSlot(sourceId,20,10,20,8,4,10,6,0,&command,&error),
            ("Keyhole-slot preparation failed: "+error.toStdString()).c_str());
    const auto preview=cad::SceneDocument::buildKeyholeSlotPreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Keyhole-slot preview invalid: "+preview.error.toStdString()).c_str());
    require(std::abs(volume(doc.parts().first().shape)-originalVolume)<1e-5,
            "Keyhole-slot preview changed the source before commit");
    const double removed=originalVolume-volume(preview.shape);
    require(removed>300&&removed<600,"Keyhole slot removed an unexpected amount of material");
    require(doc.commitKeyholeSlot(command,preview.shape,&error),
            ("Keyhole-slot commit failed: "+error.toStdString()).c_str());
    closeTo(volume(doc.parts().first().shape),volume(preview.shape));
    require(!doc.commitKeyholeSlot(command,preview.shape,&error),"Committed keyhole preview was reusable");
    require(doc.undo(),"Keyhole-slot undo failed");closeTo(volume(doc.parts().first().shape),originalVolume);
    require(doc.redo(),"Keyhole-slot redo failed");closeTo(volume(doc.parts().first().shape),originalVolume-removed);

    cad::SceneDocument rotatedDoc;
    require(rotatedDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated keyhole fixture failed");
    cad::KeyholeSlotCommand rotatedCommand;
    require(rotatedDoc.prepareKeyholeSlot(rotatedDoc.parts().first().id,20,10,20,8,4,10,5,90,
                                           &rotatedCommand,&error),"Rotated keyhole preparation failed");
    const auto rotatedPreview=cad::SceneDocument::buildKeyholeSlotPreview(rotatedCommand);
    require(!rotatedPreview.shape.IsNull()&&BRepCheck_Analyzer(rotatedPreview.shape).IsValid(),
            ("Rotated keyhole preview invalid: "+rotatedPreview.error.toStdString()).c_str());
    require(volume(rotatedPreview.shape)<volume(rotatedDoc.parts().first().shape),
            "Rotated keyhole slot did not remove material");

    cad::KeyholeSlotCommand missedCommand;
    require(rotatedDoc.prepareKeyholeSlot(rotatedDoc.parts().first().id,200,200,20,8,4,10,5,0,
                                           &missedCommand,&error),"Missed keyhole preparation failed early");
    require(cad::SceneDocument::buildKeyholeSlotPreview(missedCommand).shape.IsNull(),
            "Keyhole cutter outside the body produced a preview");

    cad::SceneDocument staleDoc;
    require(staleDoc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Stale keyhole fixture failed");
    cad::KeyholeSlotCommand staleCommand;
    require(staleDoc.prepareKeyholeSlot(staleDoc.parts().first().id,20,10,20,8,4,10,6,0,
                                         &staleCommand,&error),"Stale keyhole preparation failed");
    const auto stalePreview=cad::SceneDocument::buildKeyholeSlotPreview(staleCommand);
    require(!stalePreview.shape.IsNull(),"Stale keyhole preview failed");
    require(staleDoc.translate(staleDoc.parts().first().id,1,0,0,&error),"Stale keyhole mutation failed");
    require(!staleDoc.commitKeyholeSlot(staleCommand,stalePreview.shape,&error),
            "Stale keyhole preview committed after the body changed");
}
void testRoundedSlotPattern(){
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rounded-slot pattern source box creation failed");
    const QString id=doc.parts().first().id;
    const double original=volume(doc.parts().first().shape);
    cad::RoundedSlotPatternCommand command;
    require(!doc.prepareRoundedSlotPattern(id,8,15,20,8,18,6,0,1,12,0,&command,&error),"Rounded-slot pattern accepted one instance");
    require(!doc.prepareRoundedSlotPattern(id,8,15,20,8,18,6,0,3,0,0,&command,&error),"Rounded-slot pattern accepted zero spacing");
    require(!doc.prepareRoundedSlotPattern(id,8,15,20,8,7,6,0,3,12,0,&command,&error),"Rounded-slot pattern accepted length below width");
    require(!doc.prepareRoundedSlotPattern(id,8,15,20,8,18,6,361,3,12,0,&command,&error),"Rounded-slot pattern accepted invalid angle");
    require(!doc.prepareRoundedSlotPattern("missing",8,15,20,8,18,6,0,3,12,0,&command,&error),"Rounded-slot pattern accepted unknown body");
    require(std::abs(volume(doc.parts().first().shape)-original)<1e-5,"Rejected rounded-slot inputs modified source body");

    require(doc.prepareRoundedSlotPattern(id,8,15,20,8,18,6,0,3,12,0,&command,&error),
            ("Rounded-slot pattern preparation failed: "+error.toStdString()).c_str());
    const auto preview=cad::SceneDocument::buildRoundedSlotPatternPreview(command);
    require(!preview.shape.IsNull()&&BRepCheck_Analyzer(preview.shape).IsValid(),
            ("Rounded-slot pattern preview invalid: "+preview.error.toStdString()).c_str());
    require(std::abs(volume(doc.parts().first().shape)-original)<1e-5,"Pattern preview modified source before commit");
    const double removed=original-volume(preview.shape);
    require(removed>1800&&removed<3000,"Pattern removed an unexpected amount of material");
    require(doc.commitRoundedSlotPattern(command,preview.shape,&error),
            ("Rounded-slot pattern commit failed: "+error.toStdString()).c_str());
    closeTo(volume(doc.parts().first().shape),volume(preview.shape));
    require(!doc.commitRoundedSlotPattern(command,preview.shape,&error),"Committed rounded-slot pattern preview was reusable");
    require(doc.undo(),"Rounded-slot pattern undo failed");closeTo(volume(doc.parts().first().shape),original);
    require(doc.redo(),"Rounded-slot pattern redo failed");closeTo(volume(doc.parts().first().shape),original-removed);

    cad::SceneDocument rotated;
    require(rotated.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Rotated rounded-slot pattern fixture failed");
    cad::RoundedSlotPatternCommand rotatedCommand;
    require(rotated.prepareRoundedSlotPattern(rotated.parts().first().id,20,9,20,6,14,5,90,2,0,12,&rotatedCommand,&error),
            "Rotated rounded-slot pattern preparation failed");
    const auto rotatedPreview=cad::SceneDocument::buildRoundedSlotPatternPreview(rotatedCommand);
    require(!rotatedPreview.shape.IsNull()&&BRepCheck_Analyzer(rotatedPreview.shape).IsValid(),
            ("Rotated rounded-slot pattern invalid: "+rotatedPreview.error.toStdString()).c_str());
    require(volume(rotatedPreview.shape)<volume(rotated.parts().first().shape),"Rotated pattern did not remove material");

    cad::RoundedSlotPatternCommand missed;
    require(doc.prepareRoundedSlotPattern(id,200,200,20,8,18,6,0,2,12,0,&missed,&error),"Missed pattern setup failed early");
    require(cad::SceneDocument::buildRoundedSlotPatternPreview(missed).shape.IsNull(),"Pattern outside body produced a preview");

    cad::SceneDocument stale;
    require(stale.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Stale pattern fixture failed");
    cad::RoundedSlotPatternCommand staleCommand;
    require(stale.prepareRoundedSlotPattern(stale.parts().first().id,8,15,20,8,18,6,0,3,12,0,&staleCommand,&error),"Stale pattern preparation failed");
    const auto stalePreview=cad::SceneDocument::buildRoundedSlotPatternPreview(staleCommand);
    require(!stalePreview.shape.IsNull(),"Stale pattern preview setup failed");
    require(stale.translate(stale.parts().first().id,1,0,0,&error),"Stale pattern mutation failed");
    require(!stale.commitRoundedSlotPattern(staleCommand,stalePreview.shape,&error),"Stale rounded-slot pattern was committed");
}
void execute() {
    QTemporaryDir dir;
    require(dir.isValid(),"Temporary directory unavailable");
    QString error;
    cad::SceneDocument doc;
    require(!doc.dirty(),"Empty document dirty");
    require(!doc.addPrimitive(cad::Primitive::Box,{0,10,10},&error),"Zero width accepted");
    require(!doc.addPrimitive(cad::Primitive::Sphere,{std::numeric_limits<double>::quiet_NaN()},&error),"NaN accepted");
    require(!doc.addPrimitive(cad::Primitive::Torus,{5,6},&error),"Self-overlapping torus accepted");
    require(!doc.addPrimitive(cad::Primitive::Cone,{10,3},&error),"Incomplete dimensions accepted");
    require(doc.parts().isEmpty() && !doc.canUndo(),"Invalid primitive mutated document");

    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Box creation failed");
    const QString box = doc.parts().last().id;
    closeTo(volume(doc.parts().last().shape),6000);
    require(doc.addPrimitive(cad::Primitive::Cylinder,{3,10},&error),"Cylinder creation failed");
    const QString cylinder = doc.parts().last().id;
    closeTo(volume(doc.parts().last().shape),std::numbers::pi*90);
    require(doc.addPrimitive(cad::Primitive::Sphere,{4},&error),"Sphere creation failed");
    closeTo(volume(doc.parts().last().shape),4./3.*std::numbers::pi*64);
    require(doc.addPrimitive(cad::Primitive::Cone,{6,2,10},&error),"Cone creation failed");
    closeTo(volume(doc.parts().last().shape),std::numbers::pi*10*(36+12+4)/3);
    require(doc.addPrimitive(cad::Primitive::Torus,{12,2},&error),"Torus creation failed");
    closeTo(volume(doc.parts().last().shape),2*std::numbers::pi*std::numbers::pi*12*4,1e-3);
    for (const auto& part : doc.parts()) validate(part);
    require(doc.parts().size()==5,"Primitive count mismatch");

    require(doc.translate(cylinder,4,4,0,&error),"Translation failed");
    require(doc.rotate(box,cad::Axis::Z,90,&error),"Rotation failed");
    closeTo(volume(doc.find(box)->shape),6000);
    require(doc.scale(box,2,&error),"Uniform scaling failed");
    closeTo(volume(doc.find(box)->shape),48000,1e-4);
    require(!doc.scale(box,0,&error),"Zero scale accepted");
    require(!doc.rotate(box,cad::Axis::X,std::numeric_limits<double>::infinity(),&error),"Infinity angle accepted");
    require(!doc.translate("not-a-body",1,0,0,&error),"Unknown body accepted");
    require(doc.duplicate(box,100,0,0,&error),"Duplicate failed");
    require(doc.parts().size()==6 && doc.parts().last().id != box,"Duplicate ID conflict");
    closeTo(volume(doc.parts().last().shape),48000,1e-4);

    require(doc.rename(box,"Machine Base",&error),"Rename failed");
    require(!doc.rename(cylinder,"machine base",&error),"Case-insensitive duplicate name accepted");
    require(doc.setVisibility(box,false,&error),"Hide failed");
    require(doc.setColor(box,"#ff781a",&error),"Color change failed");
    require(!doc.setColor(box,"#bad",&error),"Invalid color accepted");
    require(!doc.find(box)->visible && doc.find(box)->color=="#ff781a","Appearance not applied");

    const QString scene = dir.filePath("project_v2.cadscene");
    require(doc.saveAs(scene,&error),"v2 save failed");
    require(!doc.dirty(),"Saved project dirty");
    const auto savedCount=doc.parts().size();
    cad::SceneDocument reopened;
    require(reopened.load(scene,&error),"v2 load failed");
    require(reopened.parts().size()==savedCount,"Round-trip body count mismatch");
    require(!reopened.find(box)->visible && reopened.find(box)->color=="#ff781a","Appearance not persisted");
    closeTo(volume(reopened.find(box)->shape),48000,1e-4);

    // Old v0.1 files do not contain visible/color attributes; migration defaults are applied.
    QJsonObject oldRoot=QJsonDocument::fromJson(readBytes(scene)).object();
    oldRoot.insert("schema","cadworkbench.scene.v1");
    QJsonArray oldParts=oldRoot.value("parts").toArray();
    for (int i=0;i<oldParts.size();++i) {
        auto item=oldParts[i].toObject();
        item.remove("visible"); item.remove("color");
        oldParts[i]=item;
    }
    oldRoot.insert("parts",oldParts);
    const QString legacy = dir.filePath("legacy.cadscene");
    writeBytes(legacy,QJsonDocument(oldRoot).toJson());
    cad::SceneDocument migrated;
    require(migrated.load(legacy,&error),"v0.1 migration failed");
    require(migrated.find(box)->visible && migrated.find(box)->color=="#6c9dc2","Legacy defaults wrong");

    // Every rejected load must retain the current document and its save path.
    auto corrupted=oldRoot;
    auto malformedParts=corrupted.value("parts").toArray();
    auto invalid=malformedParts[0].toObject(); invalid.insert("brep","!!!not-base64!!!");
    malformedParts[0]=invalid; corrupted.insert("parts",malformedParts);
    const QString bad = dir.filePath("corrupt.cadscene");
    writeBytes(bad,QJsonDocument(corrupted).toJson());
    const QString beforePath = reopened.path();
    require(!reopened.load(bad,&error),"Corrupted shape accepted");
    require(reopened.path()==beforePath && reopened.parts().size()==savedCount,"Rejected load mutated document");

    // One multi-body delete creates exactly one undo step.
    const QString d1=reopened.parts()[1].id, d2=reopened.parts()[2].id;
    require(reopened.eraseMany({d1,d2},&error),"Batch delete failed");
    require(reopened.parts().size()==savedCount-2,"Batch delete count mismatch");
    require(reopened.undo() && reopened.parts().size()==savedCount,"Batch undo not atomic");
    require(!reopened.dirty(),"Undo to saved state should be clean");
    require(reopened.eraseMany({d1,d2},&error),"Branched batch delete failed");
    require(reopened.dirty(),"Branch after undo treated as clean");
    require(!reopened.canRedo(),"Redo should clear on branch");
    require(!reopened.eraseMany({d1,"missing"},&error),"Partial invalid batch accepted");
    require(reopened.parts().size()==savedCount-2,"Invalid batch modified document");

    // Boolean cut: a hole entirely enclosed in a rectangular solid.
    cad::SceneDocument booleanDoc;
    require(booleanDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Boolean box failed");
    const QString a=booleanDoc.parts()[0].id;
    require(booleanDoc.addPrimitive(cad::Primitive::Cylinder,{3,10},&error),"Boolean cylinder failed");
    const QString b=booleanDoc.parts()[1].id;
    require(booleanDoc.translate(b,4,4,0,&error),"Boolean cylinder offset failed");
    require(!booleanDoc.booleanOperation(cad::BooleanOp::Cut,a,a,&error),"Self-Boolean accepted");
    require(booleanDoc.booleanOperation(cad::BooleanOp::Cut,a,b,&error),"Cut failed");
    require(booleanDoc.parts().size()==1,"Cut should replace both operands");
    closeTo(volume(booleanDoc.parts().first().shape),6000.-std::numbers::pi*90.,1e-3);
    require(booleanDoc.undo() && booleanDoc.parts().size()==2,"Boolean undo failed");
    require(booleanDoc.redo() && booleanDoc.parts().size()==1,"Boolean redo failed");
    cad::SceneDocument disjoint;
    require(disjoint.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Disjoint box creation failed");
    require(disjoint.addPrimitive(cad::Primitive::Sphere,{2},&error),"Disjoint sphere creation failed");
    const QString u=disjoint.parts()[0].id, v=disjoint.parts()[1].id;
    require(disjoint.translate(v,100,0,0,&error),"Sphere translation failed");
    require(!disjoint.booleanOperation(cad::BooleanOp::Common,u,v,&error),"Empty intersection accepted");
    require(disjoint.parts().size()==2,"Empty intersection mutated document");

    // Rectangle and circle sketch profiles extrude into validated B-rep solids.
    cad::SceneDocument extrusionDoc;
    cad::ExtrusionCommand rectangleCommand;
    require(extrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,-4,40,30,20,
                                          &rectangleCommand,&error),"Rectangle extrusion preparation failed");
    const auto rectanglePreview = cad::SceneDocument::buildExtrusionPreview(rectangleCommand);
    require(!rectanglePreview.shape.IsNull(),"Rectangle extrusion preview failed");
    require(extrusionDoc.parts().isEmpty() && !extrusionDoc.canUndo(),"Rectangle preview mutated document");
    require(BRepCheck_Analyzer(rectanglePreview.shape).IsValid(),"Rectangle extrusion topology invalid");
    closeTo(volume(rectanglePreview.shape),24000,1e-5);
    require(extrusionDoc.commitExtrusion(rectangleCommand,rectanglePreview.shape,&error),"Rectangle extrusion commit failed");
    require(extrusionDoc.parts().size()==1 && extrusionDoc.canUndo(),"Rectangle extrusion was not a single undoable body");
    require(extrusionDoc.undo() && extrusionDoc.parts().isEmpty(),"Rectangle extrusion undo failed");
    require(extrusionDoc.redo() && extrusionDoc.parts().size()==1,"Rectangle extrusion redo failed");

    cad::SceneDocument circleExtrusionDoc;
    cad::ExtrusionCommand circleCommand;
    require(circleExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,2,3,20,0,25,
                                                &circleCommand,&error),"Circle extrusion preparation failed");
    const auto circlePreview = cad::SceneDocument::buildExtrusionPreview(circleCommand);
    require(!circlePreview.shape.IsNull() && BRepCheck_Analyzer(circlePreview.shape).IsValid(),"Circle extrusion preview invalid");
    closeTo(volume(circlePreview.shape),std::numbers::pi*100*25,1e-5);
    require(circleExtrusionDoc.commitExtrusion(circleCommand,circlePreview.shape,&error),"Circle extrusion commit failed");
    require(circleExtrusionDoc.parts().size()==1,"Circle extrusion body missing");

    cad::SceneDocument joinExtrusionDoc;
    require(joinExtrusionDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Join extrusion fixture failed");
    const QString joinTarget=joinExtrusionDoc.parts().first().id;
    cad::ExtrusionCommand joinCommand;
    require(joinExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,5,5,5,5,15,
                                              cad::ExtrusionMode::Join,joinTarget,&joinCommand,&error),
            "Join extrusion preparation failed");
    const auto joinPreview=cad::SceneDocument::buildExtrusionPreview(joinCommand);
    require(!joinPreview.shape.IsNull() && BRepCheck_Analyzer(joinPreview.shape).IsValid(),"Join extrusion preview invalid");
    closeTo(volume(joinPreview.shape),4125,1e-5);
    require(joinExtrusionDoc.parts().size()==1 && std::abs(volume(joinExtrusionDoc.parts().first().shape)-4000)<1e-5,
            "Join extrusion preview mutated the target");
    require(joinExtrusionDoc.commitExtrusion(joinCommand,joinPreview.shape,&error),"Join extrusion commit failed");
    require(joinExtrusionDoc.parts().size()==1,"Join extrusion created an extra body");
    closeTo(volume(joinExtrusionDoc.parts().first().shape),4125,1e-5);
    require(joinExtrusionDoc.undo(),"Join extrusion undo failed");
    closeTo(volume(joinExtrusionDoc.parts().first().shape),4000,1e-5);

    cad::SceneDocument cutExtrusionDoc;
    require(cutExtrusionDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Cut extrusion fixture failed");
    const QString cutTarget=cutExtrusionDoc.parts().first().id;
    cad::ExtrusionCommand cutCommand;
    require(cutExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,10,10,4,0,10,
                                             cad::ExtrusionMode::Cut,cutTarget,&cutCommand,&error),
            "Cut extrusion preparation failed");
    const auto cutPreview=cad::SceneDocument::buildExtrusionPreview(cutCommand);
    require(!cutPreview.shape.IsNull() && BRepCheck_Analyzer(cutPreview.shape).IsValid(),"Cut extrusion preview invalid");
    closeTo(volume(cutPreview.shape),4000-40*std::numbers::pi,1e-5);
    require(cutExtrusionDoc.commitExtrusion(cutCommand,cutPreview.shape,&error),"Cut extrusion commit failed");
    require(cutExtrusionDoc.parts().size()==1,"Cut extrusion created an extra body");
    closeTo(volume(cutExtrusionDoc.parts().first().shape),4000-40*std::numbers::pi,1e-5);
    require(cutExtrusionDoc.undo(),"Cut extrusion undo failed");
    closeTo(volume(cutExtrusionDoc.parts().first().shape),4000,1e-5);

    cad::ExtrusionCommand missingTargetCommand;
    require(!cutExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,5,5,5,
                                              cad::ExtrusionMode::Join,QStringLiteral("missing"),
                                              &missingTargetCommand,&error),"Join extrusion accepted a missing target");

    cad::SceneDocument invalidExtrusionDoc;
    cad::ExtrusionCommand invalidExtrusion;
    require(!invalidExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,0,20,10,
                                                 &invalidExtrusion,&error),"Zero-width sketch profile accepted");
    require(!invalidExtrusionDoc.prepareExtrusion(cad::SketchProfile::Circle,0,0,20,0,-10,
                                                 &invalidExtrusion,&error),"Negative extrusion height accepted");
    require(invalidExtrusionDoc.parts().isEmpty() && !invalidExtrusionDoc.canUndo(),"Rejected extrusion changed document");

    cad::SceneDocument staleExtrusionDoc;
    cad::ExtrusionCommand staleExtrusion;
    require(staleExtrusionDoc.prepareExtrusion(cad::SketchProfile::Rectangle,0,0,10,10,10,
                                               &staleExtrusion,&error),"Stale extrusion preparation failed");
    const auto staleExtrusionPreview = cad::SceneDocument::buildExtrusionPreview(staleExtrusion);
    require(!staleExtrusionPreview.shape.IsNull(),"Stale extrusion fixture preview failed");
    require(staleExtrusionDoc.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"Stale extrusion mutation setup failed");
    require(!staleExtrusionDoc.commitExtrusion(staleExtrusion,staleExtrusionPreview.shape,&error),"Stale extrusion preview committed");
    require(staleExtrusionDoc.parts().size()==1,"Rejected stale extrusion changed document");

    // A chamfer preview leaves the model unchanged until commit and commits as one undo step.
    cad::SceneDocument chamferDoc;
    require(chamferDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Chamfer fixture creation failed");
    const cad::Part* chamferPart = &chamferDoc.parts().first();
    cad::ChamferCommand chamferCommand;
    require(chamferDoc.prepareChamfer({chamferPart->id,chamferPart->geometryToken,1},0.5,
                                      &chamferCommand,&error),"Chamfer preparation failed");
    const auto chamferPreview = cad::SceneDocument::buildChamferPreview(chamferCommand);
    require(!chamferPreview.shape.IsNull() && BRepCheck_Analyzer(chamferPreview.shape).IsValid(),"Chamfer preview invalid");
    require(chamferDoc.parts().size()==1 && volume(chamferDoc.parts().first().shape)==6000,
            "Chamfer preview mutated the document");
    require(chamferDoc.commitChamfer(chamferCommand,chamferPreview.shape,&error),"Chamfer commit failed");
    require(chamferDoc.undo() && std::abs(volume(chamferDoc.parts().first().shape)-6000)<1e-5,
            "Chamfer undo failed");

    cad::SceneDocument allEdgeFilletDoc;
    require(allEdgeFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"All-edge fillet fixture failed");
    const QString allFilletId=allEdgeFilletDoc.parts().first().id;
    require(!allEdgeFilletDoc.filletAllEdges(allFilletId,100,&error),"Impossible all-edge fillet was accepted");
    require(allEdgeFilletDoc.parts().size()==1,"Rejected all-edge fillet mutated the document");
    require(allEdgeFilletDoc.filletAllEdges(allFilletId,1,&error),"All-edge fillet failed");
    validate(allEdgeFilletDoc.parts().first());
    require(volume(allEdgeFilletDoc.parts().first().shape)<1000,"All-edge fillet did not remove corner volume");
    require(allEdgeFilletDoc.undo()&&std::abs(volume(allEdgeFilletDoc.parts().first().shape)-1000)<1e-5,
            "All-edge fillet undo failed");
    cad::SceneDocument allEdgeChamferDoc;
    require(allEdgeChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"All-edge chamfer fixture failed");
    require(allEdgeChamferDoc.chamferAllEdges(allEdgeChamferDoc.parts().first().id,1,&error),"All-edge chamfer failed");
    validate(allEdgeChamferDoc.parts().first());
    require(volume(allEdgeChamferDoc.parts().first().shape)<1000,"All-edge chamfer did not remove corner volume");
    require(allEdgeChamferDoc.undo()&&std::abs(volume(allEdgeChamferDoc.parts().first().shape)-1000)<1e-5,
            "All-edge chamfer undo failed");
    cad::SceneDocument draftDoc;
    require(draftDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Draft fixture failed");
    const QString draftTarget=draftDoc.parts().first().id;
    require(!draftDoc.draftFace(draftTarget,99,cad::Axis::Z,0,5,&error),"Draft accepted a nonexistent face");
    bool draftApplied=false;
    for(int faceIndex=1;faceIndex<=6&&!draftApplied;++faceIndex)
        draftApplied=draftDoc.draftFace(draftTarget,faceIndex,cad::Axis::Z,0,5,&error);
    require(draftApplied,"No box side face could be drafted against the base plane");
    validate(draftDoc.parts().first());
    require(std::abs(volume(draftDoc.parts().first().shape)-1000)>1e-4,"Face draft did not change the solid");
    require(draftDoc.undo()&&std::abs(volume(draftDoc.parts().first().shape)-1000)<1e-5,"Draft undo failed");

    // Selected-edge APIs: preview/commit and one-shot paths must preserve geometry and undo.
    cad::SceneDocument selectedFilletDoc;
    require(selectedFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Selected fillet fixture failed");
    const auto filletPart=selectedFilletDoc.parts().first();
    const cad::EdgeReference filletRef{filletPart.id,filletPart.geometryToken,1};
    cad::FilletCommand filletCommand;
    require(!selectedFilletDoc.prepareFillet(filletRef,0,&filletCommand,&error),"Zero selected-edge fillet radius accepted");
    require(!selectedFilletDoc.prepareFillet({filletPart.id,QStringLiteral("stale"),1},1,&filletCommand,&error),"Stale fillet edge accepted");
    require(!selectedFilletDoc.prepareFillet({filletPart.id,filletPart.geometryToken,999},1,&filletCommand,&error),"Missing fillet edge accepted");
    require(selectedFilletDoc.prepareFillet(filletRef,1,&filletCommand,&error),"Selected fillet preparation failed");
    const auto filletPreview=cad::SceneDocument::buildFilletPreview(filletCommand);
    require(!filletPreview.shape.IsNull()&&BRepCheck_Analyzer(filletPreview.shape).IsValid(),"Selected fillet preview invalid");
    require(selectedFilletDoc.parts().size()==1&&std::abs(volume(selectedFilletDoc.parts().first().shape)-1000)<1e-5,
            "Selected fillet preview mutated source");
    require(selectedFilletDoc.commitFillet(filletCommand,filletPreview.shape,&error),"Selected fillet commit failed");
    validate(selectedFilletDoc.parts().first());
    require(volume(selectedFilletDoc.parts().first().shape)<1000,"Selected fillet did not change solid");
    require(selectedFilletDoc.undo()&&std::abs(volume(selectedFilletDoc.parts().first().shape)-1000)<1e-5,
            "Selected fillet undo failed");
    cad::SceneDocument oneShotFilletDoc;
    require(oneShotFilletDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"One-shot fillet fixture failed");
    const auto oneShotFilletPart=oneShotFilletDoc.parts().first();
    require(oneShotFilletDoc.filletEdge({oneShotFilletPart.id,oneShotFilletPart.geometryToken,1},1,&error),
            "One-shot selected-edge fillet failed");
    require(oneShotFilletDoc.undo()&&std::abs(volume(oneShotFilletDoc.parts().first().shape)-1000)<1e-5,
            "One-shot selected-edge fillet undo failed");

    cad::SceneDocument oneShotChamferDoc;
    require(oneShotChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"One-shot chamfer fixture failed");
    const auto oneShotChamferPart=oneShotChamferDoc.parts().first();
    require(!oneShotChamferDoc.chamferEdge({oneShotChamferPart.id,oneShotChamferPart.geometryToken,1},0,&error),
            "Zero selected-edge chamfer distance accepted");
    require(oneShotChamferDoc.chamferEdge({oneShotChamferPart.id,oneShotChamferPart.geometryToken,1},1,&error),
            "One-shot selected-edge chamfer failed");
    validate(oneShotChamferDoc.parts().first());
    require(oneShotChamferDoc.undo()&&std::abs(volume(oneShotChamferDoc.parts().first().shape)-1000)<1e-5,
            "One-shot selected-edge chamfer undo failed");

    // Single delete, clear, and New document have independent and predictable history semantics.
    cad::SceneDocument lifecycleDoc;
    require(lifecycleDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Lifecycle fixture creation failed");
    const QString lifecycleId=lifecycleDoc.parts().first().id;
    require(lifecycleDoc.erase(lifecycleId,&error)&&lifecycleDoc.parts().isEmpty(),"Single-body erase failed");
    require(lifecycleDoc.undo()&&lifecycleDoc.parts().size()==1,"Single-body erase undo failed");
    lifecycleDoc.clear();
    require(lifecycleDoc.parts().isEmpty()&&lifecycleDoc.canUndo(),"Clear did not checkpoint removal");
    require(lifecycleDoc.undo()&&lifecycleDoc.parts().size()==1,"Clear undo failed");
    require(lifecycleDoc.saveAs(dir.filePath("lifecycle.cadscene"),&error),"Lifecycle document save failed");
    lifecycleDoc.newDocument();
    require(lifecycleDoc.parts().isEmpty()&&lifecycleDoc.path().isEmpty()&&!lifecycleDoc.dirty()&&
            !lifecycleDoc.canUndo()&&!lifecycleDoc.canRedo(),"New document did not reset document state");

    cad::SceneDocument staleChamferDoc;
    require(staleChamferDoc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Stale chamfer fixture failed");
    const cad::Part* staleChamferPart = &staleChamferDoc.parts().first();
    cad::ChamferCommand staleChamfer;
    require(staleChamferDoc.prepareChamfer({staleChamferPart->id,staleChamferPart->geometryToken,1},0.5,
                                            &staleChamfer,&error),"Stale chamfer preparation failed");
    const auto staleChamferPreview = cad::SceneDocument::buildChamferPreview(staleChamfer);
    require(!staleChamferPreview.shape.IsNull(),"Stale chamfer fixture preview failed");
    require(staleChamferDoc.translate(staleChamferPart->id,1,0,0,&error),"Stale chamfer mutation setup failed");
    require(!staleChamferDoc.commitChamfer(staleChamfer,staleChamferPreview.shape,&error),"Stale chamfer preview committed");

    // Revolve profiles support hollow full turns and partial angular sweeps.
    cad::SceneDocument revolveDoc;
    cad::RevolveCommand revolveCommand;
    require(revolveDoc.prepareRevolve(5,2,5,20,360,&revolveCommand,&error),"Revolve preparation failed");
    const auto revolvePreview=cad::SceneDocument::buildRevolvePreview(revolveCommand);
    require(!revolvePreview.shape.IsNull() && BRepCheck_Analyzer(revolvePreview.shape).IsValid(),"Full revolve preview invalid");
    require(revolveDoc.parts().isEmpty() && !revolveDoc.canUndo(),"Revolve preview mutated document");
    closeTo(volume(revolvePreview.shape),1500*std::numbers::pi,1e-4);
    require(revolveDoc.commitRevolve(revolveCommand,revolvePreview.shape,&error),"Revolve commit failed");
    require(revolveDoc.undo() && revolveDoc.parts().isEmpty(),"Revolve undo failed");
    require(revolveDoc.redo() && revolveDoc.parts().size()==1,"Revolve redo failed");
    cad::RevolveCommand partialRevolveCommand;
    require(revolveDoc.prepareRevolve(0,0,10,20,180,&partialRevolveCommand,&error),"Partial revolve preparation failed");
    const auto partialRevolve=cad::SceneDocument::buildRevolvePreview(partialRevolveCommand);
    require(!partialRevolve.shape.IsNull() && BRepCheck_Analyzer(partialRevolve.shape).IsValid(),"Partial revolve preview invalid");
    closeTo(volume(partialRevolve.shape),1000*std::numbers::pi,1e-4);
    require(!revolveDoc.prepareRevolve(-1,0,10,20,360,&partialRevolveCommand,&error),"Negative revolve radius accepted");
    require(!revolveDoc.prepareRevolve(0,0,10,20,361,&partialRevolveCommand,&error),"Revolve angle above 360 accepted");
    cad::RevolveCommand axisRevolveCommand;
    require(!revolveDoc.prepareRevolveAboutAxis(0,0,10,20,360,3,0,0,0,&axisRevolveCommand,&error),
            "Revolve accepted an invalid axis index");
    require(revolveDoc.prepareRevolveAboutAxis(0,0,10,20,360,0,5,0,0,&axisRevolveCommand,&error),
            "X-axis revolve preparation failed");
    const auto axisRevolvePreview=cad::SceneDocument::buildRevolvePreview(axisRevolveCommand);
    require(!axisRevolvePreview.shape.IsNull()&&BRepCheck_Analyzer(axisRevolvePreview.shape).IsValid(),"X-axis revolve invalid");
    closeTo(volume(axisRevolvePreview.shape),2000*std::numbers::pi,1e-4);
    Bnd_Box axisRevolveBounds;BRepBndLib::AddOptimal(axisRevolvePreview.shape,axisRevolveBounds,false,false);
    double arx0=0,ary0=0,arz0=0,arx1=0,ary1=0,arz1=0;axisRevolveBounds.Get(arx0,ary0,arz0,arx1,ary1,arz1);
    closeTo(arx0,5);closeTo(arx1,25);closeTo(ary0,-10);closeTo(ary1,10);
    cad::RevolveCommand staleRevolveCommand;
    require(revolveDoc.prepareRevolve(0,0,10,20,360,&staleRevolveCommand,&error),"Stale revolve preparation failed");
    const auto staleRevolvePreview=cad::SceneDocument::buildRevolvePreview(staleRevolveCommand);
    require(!staleRevolvePreview.shape.IsNull(),"Stale revolve preview fixture failed");
    require(revolveDoc.addPrimitive(cad::Primitive::Box,{2,2,2},&error),"Stale revolve mutation setup failed");
    require(!revolveDoc.commitRevolve(staleRevolveCommand,staleRevolvePreview.shape,&error),"Stale revolve preview committed");

    // Mirror creates an independent body; linear pattern creates all copies in one undo step.
    cad::SceneDocument mirrorDoc;
    require(mirrorDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Mirror source creation failed");
    const QString mirrorSource=mirrorDoc.parts().first().id;
    require(mirrorDoc.mirror(mirrorSource,cad::Axis::X,0,&error),"Mirror operation failed");
    require(mirrorDoc.parts().size()==2 && mirrorDoc.parts().last().id!=mirrorSource,"Mirror did not create an independent body");
    validate(mirrorDoc.parts().last());
    closeTo(volume(mirrorDoc.parts().last().shape),6000);
    Bnd_Box mirroredBounds;
    BRepBndLib::Add(mirrorDoc.parts().last().shape,mirroredBounds);
    double mirrorXmin=0,mirrorYmin=0,mirrorZmin=0,mirrorXmax=0,mirrorYmax=0,mirrorZmax=0;
    mirroredBounds.Get(mirrorXmin,mirrorYmin,mirrorZmin,mirrorXmax,mirrorYmax,mirrorZmax);
    closeTo(mirrorXmin,-10); closeTo(mirrorXmax,0);
    require(mirrorDoc.undo() && mirrorDoc.parts().size()==1,"Mirror undo failed");
    require(!mirrorDoc.mirror("missing",cad::Axis::Y,0,&error),"Mirror accepted unknown body");

    cad::SceneDocument advancedTransformDoc;
    require(advancedTransformDoc.addPrimitive(cad::Primitive::Box,{2,2,2},&error),"Custom transform fixture failed");
    const QString advancedTransformId=advancedTransformDoc.parts().first().id;
    require(!advancedTransformDoc.mirrorAcrossPlane(advancedTransformId,0,0,0,0,&error),"Zero-normal mirror plane accepted");
    require(advancedTransformDoc.mirrorAcrossPlane(advancedTransformId,1,1,0,0,&error),"Arbitrary-plane mirror failed");
    require(advancedTransformDoc.parts().size()==2,"Arbitrary-plane mirror did not preserve the source");
    Bnd_Box arbitraryMirrorBounds;
    BRepBndLib::AddOptimal(advancedTransformDoc.parts().last().shape,arbitraryMirrorBounds,false,false);
    double amx0=0,amy0=0,amz0=0,amx1=0,amy1=0,amz1=0;
    arbitraryMirrorBounds.Get(amx0,amy0,amz0,amx1,amy1,amz1);
    closeTo(amx0,-2);closeTo(amx1,0);closeTo(amy0,-2);closeTo(amy1,0);
    require(advancedTransformDoc.undo()&&advancedTransformDoc.parts().size()==1,"Arbitrary-plane mirror undo failed");
    require(!advancedTransformDoc.rotateAboutAxis(advancedTransformId,0,0,0,0,0,0,90,&error),"Zero rotation axis accepted");
    require(advancedTransformDoc.rotateAboutAxis(advancedTransformId,1,1,0,0,0,1,90,&error),"Custom-axis rotation failed");
    closeTo(volume(advancedTransformDoc.find(advancedTransformId)->shape),8);
    require(advancedTransformDoc.undo(),"Custom-axis rotation undo failed");

    cad::SceneDocument rectangularPatternDoc;
    require(rectangularPatternDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Rectangular pattern fixture failed");
    const QString rectangularSource=rectangularPatternDoc.parts().first().id;
    require(!rectangularPatternDoc.rectangularPattern(rectangularSource,cad::Axis::X,2,10,cad::Axis::X,2,20,&error),
            "Rectangular pattern accepted a repeated axis");
    require(rectangularPatternDoc.rectangularPattern(rectangularSource,cad::Axis::X,2,10,cad::Axis::Y,3,20,&error),
            "Two-direction rectangular pattern failed");
    require(rectangularPatternDoc.parts().size()==6,"Rectangular pattern instance count mismatch");
    for(const auto& part:rectangularPatternDoc.parts()){validate(part);closeTo(volume(part.shape),24);}
    require(rectangularPatternDoc.undo()&&rectangularPatternDoc.parts().size()==1,"Rectangular pattern was not atomic");
    require(rectangularPatternDoc.redo()&&rectangularPatternDoc.parts().size()==6,"Rectangular pattern redo failed");

    cad::SceneDocument patternDoc;
    require(patternDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Pattern source creation failed");
    const QString patternSource=patternDoc.parts().first().id;
    require(!patternDoc.linearPattern(patternSource,cad::Axis::Y,1,10,&error),"One-instance pattern accepted");
    require(!patternDoc.linearPattern(patternSource,cad::Axis::Y,4,0,&error),"Zero-spacing pattern accepted");
    require(patternDoc.parts().size()==1 && patternDoc.canUndo(),"Invalid pattern changed document unexpectedly");
    require(patternDoc.linearPattern(patternSource,cad::Axis::Y,4,-10,&error),"Linear pattern failed");
    require(patternDoc.parts().size()==4,"Linear pattern instance count mismatch");
    QSet<QString> patternNames;
    for (const auto& part : patternDoc.parts()) {
        validate(part);
        closeTo(volume(part.shape),24);
        require(!patternNames.contains(part.name.toCaseFolded()),"Pattern produced duplicate names");
        patternNames.insert(part.name.toCaseFolded());
    }
    require(patternDoc.undo() && patternDoc.parts().size()==1,"Pattern should undo as one operation");
    require(patternDoc.redo() && patternDoc.parts().size()==4,"Pattern redo failed");

    // Circular patterns include the source as instance one and commit as one undo step.
    cad::SceneDocument circularDoc;
    require(circularDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Circular pattern source creation failed");
    const QString circularSource=circularDoc.parts().first().id;
    require(!circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,1,360,&error),"One-instance circular pattern accepted");
    require(!circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,3,0,&error),"Zero-angle circular pattern accepted");
    require(circularDoc.parts().size()==1,"Invalid circular pattern changed the document");
    require(circularDoc.circularPattern(circularSource,cad::Axis::Z,0,0,4,360,&error),"Full circular pattern failed");
    require(circularDoc.parts().size()==4,"Circular pattern instance count mismatch");
    QSet<QString> circularNames;
    for (const auto& part : circularDoc.parts()) {
        validate(part);
        closeTo(volume(part.shape),24);
        require(!circularNames.contains(part.name.toCaseFolded()),"Circular pattern produced duplicate names");
        circularNames.insert(part.name.toCaseFolded());
    }
    require(circularDoc.undo() && circularDoc.parts().size()==1,"Circular pattern undo failed");
    require(circularDoc.redo() && circularDoc.parts().size()==4,"Circular pattern redo failed");
    cad::SceneDocument partialCircularDoc;
    require(partialCircularDoc.addPrimitive(cad::Primitive::Box,{2,3,4},&error),"Partial circular source creation failed");
    require(partialCircularDoc.circularPattern(partialCircularDoc.parts().first().id,cad::Axis::Y,0,0,3,180,&error),"Partial circular pattern failed");
    require(partialCircularDoc.parts().size()==3,"Partial circular pattern instance count mismatch");

    // Vertical blind and through-all holes preview without mutation, then commit and undo atomically.
    cad::SceneDocument holeDoc;
    require(holeDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Hole source creation failed");
    const QString holeSource=holeDoc.parts().first().id;
    cad::HoleCommand blindHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,5,false,&blindHoleCommand,&error),"Blind hole preparation failed");
    const double originalHoleVolume=volume(holeDoc.parts().first().shape);
    const auto blindHole=cad::SceneDocument::buildHolePreview(blindHoleCommand);
    require(!blindHole.shape.IsNull() && BRepCheck_Analyzer(blindHole.shape).IsValid(),"Blind hole preview invalid");
    closeTo(originalHoleVolume-volume(blindHole.shape),20*std::numbers::pi,1e-4);
    closeTo(volume(holeDoc.parts().first().shape),originalHoleVolume);
    require(holeDoc.commitHole(blindHoleCommand,blindHole.shape,&error),"Blind hole commit failed");
    require(holeDoc.undo() && std::abs(volume(holeDoc.parts().first().shape)-originalHoleVolume)<1e-5,"Blind hole undo failed");
    cad::HoleCommand throughHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,1,true,&throughHoleCommand,&error),"Through-hole preparation failed");
    const auto throughHole=cad::SceneDocument::buildHolePreview(throughHoleCommand);
    require(!throughHole.shape.IsNull() && BRepCheck_Analyzer(throughHole.shape).IsValid(),"Through-hole preview invalid");
    closeTo(originalHoleVolume-volume(throughHole.shape),80*std::numbers::pi,1e-4);
    cad::HoleCommand missedHoleCommand;
    require(holeDoc.prepareHole(holeSource,100,100,20,4,5,false,&missedHoleCommand,&error),"Missed hole preparation rejected before preview");
    require(cad::SceneDocument::buildHolePreview(missedHoleCommand).shape.IsNull(),"Hole outside the body produced a preview");
    cad::HoleCommand staleHoleCommand;
    require(holeDoc.prepareHole(holeSource,10,10,20,4,5,false,&staleHoleCommand,&error),"Stale hole preparation failed");
    const auto staleHole=cad::SceneDocument::buildHolePreview(staleHoleCommand);
    require(!staleHole.shape.IsNull(),"Stale hole fixture preview failed");
    require(holeDoc.translate(holeSource,1,0,0,&error),"Stale hole mutation setup failed");
    require(!holeDoc.commitHole(staleHoleCommand,staleHole.shape,&error),"Stale hole preview committed");

    cad::SceneDocument screwHoleDoc;
    require(screwHoleDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Screw-hole fixture failed");
    const QString screwHoleTarget=screwHoleDoc.parts().first().id;
    cad::HoleCommand counterboreCommand;
    require(!screwHoleDoc.prepareCounterbore(screwHoleTarget,10,10,20,4,1,false,8,2,&counterboreCommand,&error),
            "Counterbore deeper than its blind drill accepted");
    require(screwHoleDoc.prepareCounterbore(screwHoleTarget,10,10,20,4,5,false,8,2,&counterboreCommand,&error),
            "Counterbore preparation failed");
    const auto counterborePreview=cad::SceneDocument::buildHolePreview(counterboreCommand);
    require(!counterborePreview.shape.IsNull()&&BRepCheck_Analyzer(counterborePreview.shape).IsValid(),"Counterbore preview invalid");
    closeTo(8000-volume(counterborePreview.shape),44*std::numbers::pi,1e-4);
    require(screwHoleDoc.commitHole(counterboreCommand,counterborePreview.shape,&error),"Counterbore commit failed");
    require(screwHoleDoc.undo(),"Counterbore undo failed");
    cad::HoleCommand countersinkCommand;
    require(!screwHoleDoc.prepareCountersink(screwHoleTarget,10,10,20,4,5,false,3,90,&countersinkCommand,&error),
            "Countersink head smaller than drill accepted");
    require(screwHoleDoc.prepareCountersink(screwHoleTarget,10,10,20,4,5,false,8,90,&countersinkCommand,&error),
            "Countersink preparation failed");
    const auto countersinkPreview=cad::SceneDocument::buildHolePreview(countersinkCommand);
    require(!countersinkPreview.shape.IsNull()&&BRepCheck_Analyzer(countersinkPreview.shape).IsValid(),"Countersink preview invalid");
    closeTo(8000-volume(countersinkPreview.shape),92*std::numbers::pi/3.0,1e-4);

    cad::SceneDocument holeGridDoc;
    require(holeGridDoc.addPrimitive(cad::Primitive::Box,{20,20,10},&error),"Hole-grid fixture failed");
    const QString holeGridTarget=holeGridDoc.parts().first().id;
    require(!holeGridDoc.holePattern(holeGridTarget,19,6,10,2,10,true,2,4,2,4,&error),
            "Hole grid accepted a cutter outside the body");
    require(holeGridDoc.parts().size()==1&&std::abs(volume(holeGridDoc.parts().first().shape)-4000)<1e-5,
            "Failed hole grid partially modified the target");
    require(holeGridDoc.holePattern(holeGridTarget,6,6,10,2,10,true,3,4,2,4,&error),"Six-hole grid failed");
    closeTo(4000-volume(holeGridDoc.parts().first().shape),60*std::numbers::pi,1e-4);
    require(holeGridDoc.undo()&&std::abs(volume(holeGridDoc.parts().first().shape)-4000)<1e-5,"Hole-grid undo failed");

    // Split body replaces one source with two valid pieces in one undoable operation.
    cad::SceneDocument splitDoc;
    require(splitDoc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Split source creation failed");
    const QString splitSource=splitDoc.parts().first().id;
    require(!splitDoc.splitBody(splitSource,cad::Axis::X,0,&error),"Outside split plane accepted");
    require(splitDoc.parts().size()==1,"Rejected split changed the document");
    require(splitDoc.splitBody(splitSource,cad::Axis::X,5,&error),"Split body operation failed");
    require(splitDoc.parts().size()==2,"Split body did not produce two pieces");
    require(splitDoc.parts().first().id==splitSource && splitDoc.parts().last().id!=splitSource,"Split body identity handling failed");
    for (const auto& part:splitDoc.parts()) { validate(part); closeTo(volume(part.shape),3000); }
    require(splitDoc.undo() && splitDoc.parts().size()==1,"Split body undo failed");
    require(splitDoc.redo() && splitDoc.parts().size()==2,"Split body redo failed");

    // Shell removes a planar opening face and hollows the body inward.
    cad::SceneDocument shellDoc;
    require(shellDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Shell source creation failed");
    const QString shellSource=shellDoc.parts().first().id;
    require(!shellDoc.shellBody(shellSource,cad::Axis::Z,true,0,&error),"Zero wall thickness accepted");
    require(shellDoc.shellBody(shellSource,cad::Axis::Z,true,2,&error),"Shell operation failed");
    validate(shellDoc.parts().first());
    closeTo(volume(shellDoc.parts().first().shape),3392,1e-3);
    require(shellDoc.undo() && std::abs(volume(shellDoc.parts().first().shape)-8000)<1e-5,"Shell undo failed");
    cad::SceneDocument nonPlanarShellDoc;
    require(nonPlanarShellDoc.addPrimitive(cad::Primitive::Sphere,{10},&error),"Nonplanar shell fixture failed");
    require(!nonPlanarShellDoc.shellBody(nonPlanarShellDoc.parts().first().id,cad::Axis::Z,true,1,&error),"Shell found a nonexistent planar opening face");
    cad::SceneDocument multiOpeningShellDoc;
    require(multiOpeningShellDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Multi-opening shell source creation failed");
    const QString multiOpeningShellId=multiOpeningShellDoc.parts().first().id;
    const double multiOpeningOriginalVolume=volume(multiOpeningShellDoc.parts().first().shape);
    require(!multiOpeningShellDoc.shellBodyWithOpenings(multiOpeningShellId,{},2,&error),"Multi-opening shell accepted an empty opening list");
    require(!multiOpeningShellDoc.shellBodyWithOpenings(multiOpeningShellId,{{cad::Axis::Z,true},{cad::Axis::Z,true}},2,&error),
            "Multi-opening shell accepted a duplicate face");
    require(std::abs(volume(multiOpeningShellDoc.parts().first().shape)-multiOpeningOriginalVolume)<1e-5,
            "Rejected multi-opening shells changed the source body");
    require(multiOpeningShellDoc.shellBodyWithOpenings(multiOpeningShellId,
                {{cad::Axis::Z,false},{cad::Axis::Z,true}},2,&error),("Opposite-face shell failed: "+error.toStdString()).c_str());
    validate(multiOpeningShellDoc.parts().first());
    require(volume(multiOpeningShellDoc.parts().first().shape)<multiOpeningOriginalVolume,
            "Multi-opening shell did not remove material");
    require(multiOpeningShellDoc.undo()&&std::abs(volume(multiOpeningShellDoc.parts().first().shape)-multiOpeningOriginalVolume)<1e-5,
            "Multi-opening shell undo failed");
    cad::SceneDocument adjacentOpeningShellDoc;
    require(adjacentOpeningShellDoc.addPrimitive(cad::Primitive::Box,{20,20,20},&error),"Adjacent-opening shell source creation failed");
    const QString adjacentOpeningShellId=adjacentOpeningShellDoc.parts().first().id;
    require(adjacentOpeningShellDoc.shellBodyWithOpenings(adjacentOpeningShellId,
                {{cad::Axis::X,true},{cad::Axis::Z,true}},2,&error),("Adjacent-face shell failed: "+error.toStdString()).c_str());
    validate(adjacentOpeningShellDoc.parts().first());
    require(volume(adjacentOpeningShellDoc.parts().first().shape)<8000,"Adjacent-face shell did not remove material");
    // New profile-driven solids: regular prism, capsule, ellipsoid, tube and wedge.
    cad::SceneDocument advancedPrimitives;
    require(!advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,2},&error),
            "Regular prism accepted fewer than three sides");
    require(!advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,5.5},&error),
            "Regular prism accepted fractional side count");
    require(advancedPrimitives.addPrimitive(cad::Primitive::RegularPrism,{5,10,6},&error),
            "Hexagonal prism creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),375*std::sqrt(3.0),1e-4);
    require(advancedPrimitives.addPrimitive(cad::Primitive::Capsule,{3,10},&error),
            "Capsule creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),90*std::numbers::pi+36*std::numbers::pi,1e-4);
    require(advancedPrimitives.addPrimitive(cad::Primitive::Ellipsoid,{3,4,5},&error),
            "Ellipsoid creation failed");
    validate(advancedPrimitives.parts().last());
    Bnd_Box ellipsoidBounds;
    BRepBndLib::AddOptimal(advancedPrimitives.parts().last().shape,ellipsoidBounds,false,false);
    double ex0=0,ey0=0,ez0=0,ex1=0,ey1=0,ez1=0;
    ellipsoidBounds.Get(ex0,ey0,ez0,ex1,ey1,ez1);
    // OCCT represents a general triaxial ellipsoid as a transformed spline surface;
    // its bounds and integrated volume stay within 0.5% of the analytic dimensions.
    closeTo(ex1-ex0,6,5e-3); closeTo(ey1-ey0,8,5e-3); closeTo(ez1-ez0,10,1e-5);
    closeTo(volume(advancedPrimitives.parts().last().shape),80*std::numbers::pi,5e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::Tube,{5,5,10},&error),
            "Tube accepted equal inner and outer radii");
    require(advancedPrimitives.addPrimitive(cad::Primitive::Tube,{5,2,10},&error),
            "Tube creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),210*std::numbers::pi,1e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::Wedge,{10,10,10,11},&error),
            "Wedge accepted a top wider than its base");
    require(advancedPrimitives.addPrimitive(cad::Primitive::Wedge,{10,8,6,4},&error),
            "Wedge creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),336,1e-4);
    require(advancedPrimitives.parts().size()==5,"Advanced primitive operation count mismatch");
    require(advancedPrimitives.undo() && advancedPrimitives.parts().size()==4,
            "Wedge creation was not undoable");
    require(advancedPrimitives.redo() && advancedPrimitives.parts().size()==5,
            "Wedge redo failed");
    require(!advancedPrimitives.addPrimitive(cad::Primitive::PyramidFrustum,{30,20,31,10,25},&error),
            "Pyramid frustum accepted an oversized top");
    require(advancedPrimitives.addPrimitive(cad::Primitive::PyramidFrustum,{30,20,12,10,25},&error),
            "Pyramid frustum creation failed");
    validate(advancedPrimitives.parts().last());
    // Each rectangular side tapers linearly; integrate the interpolated width*depth.
    closeTo(volume(advancedPrimitives.parts().last().shape),8250,1e-4);
    require(!advancedPrimitives.addPrimitive(cad::Primitive::HexNut,{10,9,8},&error),
            "Hex nut accepted a bore that breaks through a flat");
    require(advancedPrimitives.addPrimitive(cad::Primitive::HexNut,{10,4,8},&error),
            "Hex nut creation failed");
    validate(advancedPrimitives.parts().last());
    closeTo(volume(advancedPrimitives.parts().last().shape),(150*std::sqrt(3.0)-16*std::numbers::pi)*8,1e-4);
    require(advancedPrimitives.parts().size()==7,"New engineering solid count mismatch");

    cad::SceneDocument orientedPrimitiveDoc;
    require(!orientedPrimitiveDoc.addOrientedCylinder(2,10,1,2,3,0,0,0,&error),"Oriented cylinder accepted a zero axis");
    require(orientedPrimitiveDoc.addOrientedCylinder(2,10,1,2,3,1,0,0,&error),"Oriented cylinder creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),40*std::numbers::pi);
    Bnd_Box orientedCylinderBounds;BRepBndLib::AddOptimal(orientedPrimitiveDoc.parts().last().shape,orientedCylinderBounds,false,false);
    double ocx0=0,ocy0=0,ocz0=0,ocx1=0,ocy1=0,ocz1=0;orientedCylinderBounds.Get(ocx0,ocy0,ocz0,ocx1,ocy1,ocz1);
    closeTo(ocx0,1);closeTo(ocx1,11);closeTo(ocy0,0);closeTo(ocy1,4);closeTo(ocz0,1);closeTo(ocz1,5);
    require(orientedPrimitiveDoc.addOrientedCone(3,1,6,0,0,0,0,0,1,&error),"Oriented cone creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),26*std::numbers::pi);
    require(orientedPrimitiveDoc.addTorusSegment(10,2,90,&error),"Torus segment creation failed");
    validate(orientedPrimitiveDoc.parts().last());closeTo(volume(orientedPrimitiveDoc.parts().last().shape),20*std::numbers::pi*std::numbers::pi,1e-4);
    require(!orientedPrimitiveDoc.addTorusSegment(10,2,361,&error),"Torus sweep above 360 degrees accepted");

    // Additional closed sketch profiles extrude into solids with analytic volumes.
    const double profileHeight=12.0;
    cad::ExtrusionCommand ellipseCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ellipse,0,0,20,25,profileHeight,
                                                 &ellipseCommand,&error),"Ellipse accepted minor diameter larger than major");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ellipse,1,2,20,10,profileHeight,
                                                &ellipseCommand,&error),"Ellipse extrusion preparation failed");
    const auto ellipsePreview=cad::SceneDocument::buildExtrusionPreview(ellipseCommand);
    require(!ellipsePreview.shape.IsNull() && BRepCheck_Analyzer(ellipsePreview.shape).IsValid(),"Ellipse extrusion invalid");
    closeTo(volume(ellipsePreview.shape),600*std::numbers::pi,1e-4);
    require(advancedPrimitives.commitExtrusion(ellipseCommand,ellipsePreview.shape,&error),"Ellipse extrusion commit failed");

    cad::ExtrusionCommand slotCommand;
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Slot,0,0,30,10,profileHeight,
                                                 &slotCommand,&error),"Slot extrusion preparation failed");
    const auto slotPreview=cad::SceneDocument::buildExtrusionPreview(slotCommand);
    require(!slotPreview.shape.IsNull() && BRepCheck_Analyzer(slotPreview.shape).IsValid(),"Slot extrusion invalid");
    closeTo(volume(slotPreview.shape),(200+25*std::numbers::pi)*profileHeight,1e-4);

    cad::ExtrusionCommand triangleCommand;
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Triangle,0,0,20,0,profileHeight,
                                                &triangleCommand,&error),"Triangle extrusion preparation failed");
    const auto trianglePreview=cad::SceneDocument::buildExtrusionPreview(triangleCommand);
    require(!trianglePreview.shape.IsNull() && BRepCheck_Analyzer(trianglePreview.shape).IsValid(),"Triangle extrusion invalid");
    closeTo(volume(trianglePreview.shape),900*std::sqrt(3.0),1e-4);

    cad::ExtrusionCommand polygonCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::RegularPolygon,0,0,20,2.5,profileHeight,
                                                 &polygonCommand,&error),"Polygon accepted fractional side count");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::RegularPolygon,0,0,20,8,profileHeight,
                                                 &polygonCommand,&error),"Octagon extrusion preparation failed");
    const auto polygonPreview=cad::SceneDocument::buildExtrusionPreview(polygonCommand);
    require(!polygonPreview.shape.IsNull() && BRepCheck_Analyzer(polygonPreview.shape).IsValid(),"Regular polygon extrusion invalid");
    closeTo(volume(polygonPreview.shape),2400*std::sqrt(2.0),1e-4);

    cad::ExtrusionCommand symmetricCommand;
    require(advancedPrimitives.prepareSymmetricExtrusion(cad::SketchProfile::Rectangle,0,0,10,6,8,
        cad::ExtrusionMode::NewBody,QString(),&symmetricCommand,&error),"Symmetric extrusion preparation failed");
    const auto symmetricPreview=cad::SceneDocument::buildExtrusionPreview(symmetricCommand);
    require(!symmetricPreview.shape.IsNull()&&BRepCheck_Analyzer(symmetricPreview.shape).IsValid(),"Symmetric extrusion invalid");
    closeTo(volume(symmetricPreview.shape),480);
    Bnd_Box symmetricBounds;BRepBndLib::AddOptimal(symmetricPreview.shape,symmetricBounds,false,false);
    double sx0=0,sy0=0,sz0=0,sx1=0,sy1=0,sz1=0;symmetricBounds.Get(sx0,sy0,sz0,sx1,sy1,sz1);
    closeTo(sz0,-4);closeTo(sz1,4);
    require(advancedPrimitives.commitExtrusion(symmetricCommand,symmetricPreview.shape,&error),"Symmetric extrusion commit failed");
    require(advancedPrimitives.undo(),"Symmetric extrusion undo failed");

    cad::ExtrusionCommand taperedCommand;
    require(!advancedPrimitives.prepareTaperedExtrusion(cad::SketchProfile::Rectangle,0,0,10,6,20,45,
        cad::ExtrusionMode::NewBody,QString(),&taperedCommand,&error),"Collapsing taper accepted");
    require(advancedPrimitives.prepareTaperedExtrusion(cad::SketchProfile::Rectangle,0,0,10,8,5,10,
        cad::ExtrusionMode::NewBody,QString(),&taperedCommand,&error),"Tapered extrusion preparation failed");
    const auto taperedPreview=cad::SceneDocument::buildExtrusionPreview(taperedCommand);
    require(!taperedPreview.shape.IsNull()&&BRepCheck_Analyzer(taperedPreview.shape).IsValid(),"Tapered extrusion invalid");
    const double tw=10-10*std::tan(std::numbers::pi/18.0),td=8-10*std::tan(std::numbers::pi/18.0);
    const double expectedTaperVolume=5.0/6.0*(2*80+10*td+tw*8+2*tw*td);
    closeTo(volume(taperedPreview.shape),expectedTaperVolume,1e-4);

    cad::ExtrusionCommand roundedCommand;
    require(!advancedPrimitives.prepareRoundedRectangleExtrusion(0,0,20,10,5,5,cad::ExtrusionMode::NewBody,QString(),&roundedCommand,&error),
            "Rounded rectangle accepted a corner radius too large for the profile");
    require(advancedPrimitives.prepareRoundedRectangleExtrusion(0,0,20,10,2,5,cad::ExtrusionMode::NewBody,QString(),&roundedCommand,&error),
            "Rounded rectangle extrusion preparation failed");
    const auto roundedPreview=cad::SceneDocument::buildExtrusionPreview(roundedCommand);
    require(!roundedPreview.shape.IsNull()&&BRepCheck_Analyzer(roundedPreview.shape).IsValid(),"Rounded rectangle extrusion invalid");
    closeTo(volume(roundedPreview.shape),(200-(4-std::numbers::pi)*4)*5,1e-4);

    cad::ExtrusionCommand ringCommand;
    require(!advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ring,0,0,20,20,4,&ringCommand,&error),
            "Ring accepted equal inner and outer diameters");
    require(advancedPrimitives.prepareExtrusion(cad::SketchProfile::Ring,0,0,20,10,4,&ringCommand,&error),
            "Ring extrusion preparation failed");
    const auto ringPreview=cad::SceneDocument::buildExtrusionPreview(ringCommand);
    require(!ringPreview.shape.IsNull()&&BRepCheck_Analyzer(ringPreview.shape).IsValid(),"Ring extrusion invalid");
    closeTo(volume(ringPreview.shape),300*std::numbers::pi,1e-4);

    cad::SceneDocument loftDoc;
    require(!loftDoc.loftRectangles(0,0,10,8,1,1,6,4,0,&error),"Zero-height loft accepted");
    require(loftDoc.loftRectangles(0,0,10,8,2,2,6,4,20,&error),"Rectangular-section loft failed");
    require(loftDoc.parts().size()==1,"Loft did not create one body");
    validate(loftDoc.parts().first());
    closeTo(volume(loftDoc.parts().first().shape),20.0/6.0*(2*80+10*4+6*8+2*24),1e-4);
    require(loftDoc.undo()&&loftDoc.parts().isEmpty(),"Loft undo failed");

    // v1.3 adds thirty engineering components. Exercise every default build,
    // topology/volume guard, scene transaction, and undo path.
    for(int index=0;index<static_cast<int>(cad::EngineeringPart::Count);++index){
        const auto kind=static_cast<cad::EngineeringPart>(index);
        const auto spec=cad::engineeringPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Engineering component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addEngineeringPart(kind,invalidDimensions,&error),"Engineering component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid engineering component mutated its document");
        const bool componentCreated=componentDoc.addEngineeringPart(kind,spec.defaults,&error);
        const std::string componentError="Engineering component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Engineering component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Engineering component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Engineering component undo failed");
    }

    // v1.4 adds thirty advanced production and machine components. Validate every
    // default model, malformed dimension handling, a single document commit, and undo.
    for(int index=0;index<static_cast<int>(cad::AdvancedPart::Count);++index){
        const auto kind=static_cast<cad::AdvancedPart>(index);
        const auto spec=cad::advancedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Advanced component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addAdvancedPart(kind,invalidDimensions,&error),"Advanced component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid advanced component mutated its document");
        const bool componentCreated=componentDoc.addAdvancedPart(kind,spec.defaults,&error);
        const std::string componentError="Advanced component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Advanced component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Advanced component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Advanced component undo failed");
    }

    // v1.5 adds thirty-five specialized drivetrain, bearing, actuator, engine,
    // coupling and piping models; exercise defaults, rejection, validity and undo.
    for(int index=0;index<static_cast<int>(cad::SpecializedPart::Count);++index){
        const auto kind=static_cast<cad::SpecializedPart>(index);
        const auto spec=cad::specializedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Specialized component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addSpecializedPart(kind,invalidDimensions,&error),"Specialized component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid specialized component mutated its document");
        const bool componentCreated=componentDoc.addSpecializedPart(kind,spec.defaults,&error);
        const std::string componentError="Specialized component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Specialized component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Specialized component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Specialized component undo failed");
    }

    // v1.6 adds seventy industrial components. Exercise all default builds,
    // malformed dimensions, topology/volume guards and transactional undo.
    for(int index=0;index<static_cast<int>(cad::IndustrialPart::Count);++index){
        const auto kind=static_cast<cad::IndustrialPart>(index);
        const auto spec=cad::industrialPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Industrial component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addIndustrialPart(kind,invalidDimensions,&error),"Industrial component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid industrial component mutated its document");
        const bool componentCreated=componentDoc.addIndustrialPart(kind,spec.defaults,&error);
        const std::string componentError="Industrial component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Industrial component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Industrial component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Industrial component undo failed");
    }

    // v1.7 contributes seventy more extended component templates. Check every
    // default build, zero-dimension rejection, valid topology/volume and undo.
    for(int index=0;index<static_cast<int>(cad::ExtendedPart::Count);++index){
        const auto kind=static_cast<cad::ExtendedPart>(index);
        const auto spec=cad::extendedPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Extended component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addExtendedPart(kind,invalidDimensions,&error),"Extended component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid extended component mutated its document");
        const bool componentCreated=componentDoc.addExtendedPart(kind,spec.defaults,&error);
        const std::string componentError="Extended component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Extended component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Extended component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Extended component undo failed");
    }

    // v1.8 adds seventy precision and utility component templates. Exercise every
    // default, zero-dimension rejection, valid topology/volume, and undo.
    for(int index=0;index<static_cast<int>(cad::PrecisionPart::Count);++index){
        const auto kind=static_cast<cad::PrecisionPart>(index);
        const auto spec=cad::precisionPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Precision component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addPrecisionPart(kind,invalidDimensions,&error),"Precision component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid precision component mutated its document");
        const bool componentCreated=componentDoc.addPrecisionPart(kind,spec.defaults,&error);
        const std::string componentError="Precision component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Precision component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Precision component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Precision component undo failed");
    }

    // v1.9 adds one hundred production-component templates. Exercise every default,
    // zero-dimension rejection, valid topology/volume and undo.
    for(int index=0;index<static_cast<int>(cad::ProductionPart::Count);++index){
        const auto kind=static_cast<cad::ProductionPart>(index);
        const auto spec=cad::productionPartSpec(kind);
        require(!spec.name.isEmpty()&&spec.defaults.size()==spec.labels.size(),"Production component specification is incomplete");
        cad::SceneDocument componentDoc;
        auto invalidDimensions=spec.defaults; invalidDimensions[0]=0;
        require(!componentDoc.addProductionPart(kind,invalidDimensions,&error),"Production component accepted a zero dimension");
        require(componentDoc.parts().isEmpty()&&!componentDoc.canUndo(),"Invalid production component mutated its document");
        const bool componentCreated=componentDoc.addProductionPart(kind,spec.defaults,&error);
        const std::string componentError="Production component failed: "+spec.name.toStdString()+"; "+error.toStdString();
        require(componentCreated,componentError.c_str());
        require(componentDoc.parts().size()==1&&componentDoc.parts().first().name==spec.name,"Production component was not added with its expected name");
        validate(componentDoc.parts().first());
        require(volume(componentDoc.parts().first().shape)>0,"Production component has no positive volume");
        require(componentDoc.undo()&&componentDoc.parts().isEmpty(),"Production component undo failed");
    }

    const QString stl=dir.filePath("solid.stl"),step=dir.filePath("solid.step");
    require(booleanDoc.exportSTEP(step,&error) && QFileInfo(step).size()>100,"STEP export failed");
    require(booleanDoc.exportSTL(stl,&error) && QFileInfo(stl).size()>84,"STL export failed");
    require(!cad::SceneDocument{}.exportSTL(stl,&error),"Empty STL export accepted");
    cad::SceneDocument stepSource;
    require(stepSource.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create STEP import test box");
    require(stepSource.addPrimitive(cad::Primitive::Cylinder,{5,8},&error),"Could not create STEP import test cylinder");
    const QString assemblyStep=dir.filePath("assembly.step");
    require(stepSource.exportSTEP(assemblyStep,&error),"Multi-body STEP fixture export failed");
    cad::SceneDocument stepTarget;
    require(stepTarget.addPrimitive(cad::Primitive::Sphere,{3},&error),"Could not create preexisting body for STEP append test");
    const double expectedImportedVolume=volume(stepSource.parts()[0].shape)+volume(stepSource.parts()[1].shape);
    require(stepTarget.importSTEP(assemblyStep,&error),("STEP import failed: "+error.toStdString()).c_str());
    require(stepTarget.parts().size()==3,"STEP import did not append both solid bodies");
    require(stepTarget.parts()[0].name=="Sphere"&&stepTarget.parts()[1].name=="Box"&&stepTarget.parts()[2].name=="Cylinder",
            "STEP import did not preserve the existing body and source component names");
    require(!stepTarget.parts()[1].componentDefinitionId.isEmpty()&&!stepTarget.parts()[2].componentDefinitionId.isEmpty()&&
            !stepTarget.parts()[1].occurrenceId.isEmpty()&&!stepTarget.parts()[2].occurrenceId.isEmpty()&&
            stepTarget.parts()[1].occurrenceId!=stepTarget.parts()[2].occurrenceId&&
            stepTarget.parts()[1].assemblyPath=="assembly"&&stepTarget.parts()[2].assemblyPath=="assembly",
            "STEP import did not create identifiable occurrence records within the imported assembly path");
    validate(stepTarget.parts()[1]); validate(stepTarget.parts()[2]);
    require(std::abs(volume(stepTarget.parts()[1].shape)+volume(stepTarget.parts()[2].shape)-expectedImportedVolume)<1e-4,"STEP import changed the multi-body volume");
    require(stepTarget.dirty()&&stepTarget.canUndo(),"STEP import was not tracked as an undoable document change");
    require(stepTarget.undo()&&stepTarget.parts().size()==1&&stepTarget.parts()[0].name=="Sphere","STEP import undo did not restore the existing document");
    const QString invalidStep=dir.filePath("invalid.step");
    QFile invalidStepFile(invalidStep); require(invalidStepFile.open(QIODevice::WriteOnly),"Could not create invalid STEP test input");
    invalidStepFile.write("not a STEP file"); invalidStepFile.close();
    require(!stepTarget.importSTEP(invalidStep,&error)&&stepTarget.parts().size()==1,"Invalid STEP import mutated the existing document");
    testSweeps();
    testHelicalSweep();
    std::cout << "PASS: " << checks << " C++ assertions; primitives, 30 v1.3 engineering components, 30 v1.4 advanced components, 35 v1.5 specialized components, 70 v1.6 industrial components, 70 v1.7 extended components, 70 v1.8 precision components, 100 v1.9 production components, transforms, duplicates, metadata, "
              << "undo branches, atomic batch deletion, v1 migration, v2 save/load, "
              << "Boolean validity, fillet/chamfer transactions, profile extrusion/revolve preview and staleness, "
              << "mirror, linear/circular patterns, hole previews, split body, and shell with undo, "
              << "corrupt rollback, STEP import/export, circular/rectangular sweeps, and STL output\n";
}
void testStepAssemblyImport() {
    QString error;
    QTemporaryDir dir;require(dir.isValid(),"STEP assembly test folder failed");
    cad::SceneDocument source;
    require(source.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create STEP assembly test box");
    require(source.addPrimitive(cad::Primitive::Cylinder,{5,8},&error),"Could not create STEP assembly test cylinder");
    const QString path=dir.filePath("assembly.step");
    require(source.exportSTEP(path,&error),"Could not export STEP assembly fixture");
    cad::SceneDocument imported;
    require(imported.importSTEP(path,&error),("STEP assembly import failed: "+error.toStdString()).c_str());
    require(imported.parts().size()==2,"STEP assembly import did not retain both solids");
    for(const cad::Part& part:imported.parts()){
        validate(part);
        require(part.assemblyPath=="assembly"&&!part.componentDefinitionId.isEmpty()&&
                !part.componentName.isEmpty()&&!part.occurrenceId.isEmpty(),
                "STEP assembly import lost component identity or hierarchy path");
    }
    require(imported.parts()[0].occurrenceId!=imported.parts()[1].occurrenceId,
            "Different STEP solids received the same occurrence ID");
    closeTo(volume(imported.parts()[0].shape)+volume(imported.parts()[1].shape),
            volume(source.parts()[0].shape)+volume(source.parts()[1].shape));
    require(imported.undo()&&imported.parts().isEmpty(),"STEP assembly import could not be undone atomically");

    cad::SceneDocument assembly;require(assembly.addPrimitive(cad::Primitive::Box,{10,12,14},&error),"Could not create reusable STEP component");
    const QString sourceId=assembly.parts().first().id;
    require(assembly.makeComponent(sourceId,"Bracket",&error)&&assembly.insertComponentOccurrence(sourceId,25,4,0,&error),
            "Could not construct repeated, translated STEP component occurrences");
    require(assembly.setPartMetadata(assembly.parts()[0].id,"BRK-001","Mounting bracket","A","Machine/Drive/Brackets",&error)&&
            assembly.setPartMetadata(assembly.parts()[1].id,"BRK-001","Mounting bracket","A","Machine/Drive/Brackets",&error),
            "Could not assign nested STEP assembly path metadata");
    const QString repeatedPath=dir.filePath("repeated.step");require(assembly.exportSTEP(repeatedPath,&error),"Could not export reusable component assembly");
    cad::SceneDocument reloaded;require(reloaded.importSTEP(repeatedPath,&error),"Could not re-import reusable component assembly");
    require(reloaded.parts().size()==2&&reloaded.parts()[0].componentDefinitionId==reloaded.parts()[1].componentDefinitionId&&
            reloaded.parts()[0].occurrenceId!=reloaded.parts()[1].occurrenceId,
            "STEP round trip did not preserve a shared definition with distinct component occurrences");
    require(reloaded.parts()[0].componentName=="Bracket"&&reloaded.parts()[1].componentName=="Bracket",
            "STEP round trip changed the reusable component definition name");
    require(reloaded.parts()[0].assemblyPath.endsWith("Machine/Drive/Brackets")&&
            reloaded.parts()[1].assemblyPath.endsWith("Machine/Drive/Brackets"),
            ("STEP round trip lost the nested assembly hierarchy path: '"+reloaded.parts()[0].assemblyPath.toStdString()+"' / '"+
             reloaded.parts()[1].assemblyPath.toStdString()+"'" ).c_str());
    Bnd_Box firstBounds,secondBounds;BRepBndLib::Add(reloaded.parts()[0].shape,firstBounds);BRepBndLib::Add(reloaded.parts()[1].shape,secondBounds);
    double ax=0,ay=0,az=0,bx=0,by=0,bz=0;firstBounds.Get(ax,ay,az,bx,by,bz);
    double cx=0,cy=0,cz=0,dx=0,dy=0,dz=0;secondBounds.Get(cx,cy,cz,dx,dy,dz);
    closeTo(ax,0);closeTo(cx,25);closeTo(cy,4);closeTo(dx-cx,10);closeTo(dy-cy,12);
}
void testStlMeshImport() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"STL mesh import test folder failed");
    cad::SceneDocument source;require(source.addPrimitive(cad::Primitive::Box,{40,30,20},&error),"Could not create STL source solid");
    const QString stlPath=dir.filePath("faceted-box.stl");require(source.exportSTL(stlPath,&error),"Could not create binary STL import fixture");
    cad::SceneDocument imported;
    cad::MeshImportSummary repairSummary;
    const bool stlImported=imported.importSTL(stlPath,0.01,&error,&repairSummary);
    require(stlImported,("Closed STL mesh import failed: "+error.toStdString()).c_str());
    require(repairSummary.inputTriangles>=12&&repairSummary.outputTriangles==12&&repairSummary.outputVertices==8&&
            repairSummary.weldedVertices>0,"STL import did not report its split-facet vertex cleanup counts");
    require(imported.parts().size()==1&&imported.parts().first().name=="faceted-box","STL importer did not create one named faceted body");
    validate(imported.parts().first());closeTo(volume(imported.parts().first().shape),24000,1e-5);
    cad::BodyDiagnostics diagnostics;require(imported.diagnoseBody(imported.parts().first().id,&diagnostics,&error),"Imported STL solid diagnostics failed");
    require(diagnostics.faces==12&&diagnostics.solids==1,"STL import did not preserve the box as a triangular faceted solid");
    require(imported.undo()&&imported.parts().isEmpty(),"STL mesh import could not be undone atomically");
    require(imported.redo()&&imported.parts().size()==1,"STL mesh import could not be redone");
    const cad::MeshImportSummary successfulSummary=repairSummary;
    require(!imported.importSTL(dir.filePath("missing.stl"),0.01,&error,&repairSummary)&&
            imported.parts().size()==1&&repairSummary==successfulSummary,
            "Missing STL input changed the current scene");
    require(!imported.importSTL(stlPath,0,&error,&repairSummary)&&imported.parts().size()==1&&repairSummary==successfulSummary,
            "Invalid STL weld tolerance changed the scene or mesh report");
    const QString openPath=dir.filePath("open-triangle.stl");QFile openFile(openPath);
    require(openFile.open(QIODevice::WriteOnly),"Could not create open STL fixture");
    const QByteArray openStl="solid open\nfacet normal 0 0 1\n outer loop\n vertex 0 0 0\n vertex 1 0 0\n vertex 0 1 0\n endloop\nendfacet\nendsolid open\n";
    require(openFile.write(openStl)==openStl.size(),"Could not write open STL fixture");openFile.close();
    require(!imported.importSTL(openPath,0.01,&error)&&imported.parts().size()==1,
            "Open STL surface was accepted as a solid or partially modified the scene");
    cad::SceneDocument openSurface;cad::MeshImportSummary openSummary;
    require(openSurface.importSTL(openPath,0.01,&error,&openSummary,false,true)&&
            openSurface.parts().size()==1&&openSurface.parts().first().isSurfaceBody&&
            openSurface.parts().first().shape.ShapeType()==TopAbs_SHELL&&openSummary.importedAsSurfaceBody,
            ("Open STL could not be retained as a faceted surface body: "+error.toStdString()).c_str());
    const QString openScene=dir.filePath("open-stl-surface.cadscene");
    require(openSurface.saveAs(openScene,&error),"Could not save an imported open STL surface body");
    cad::SceneDocument reopenedSurface;require(reopenedSurface.load(openScene,&error)&&
        reopenedSurface.parts().size()==1&&reopenedSurface.parts().first().isSurfaceBody&&
        reopenedSurface.parts().first().shape.ShapeType()==TopAbs_SHELL,
        "Imported open STL surface body did not survive save/reopen");
}
void testIgesExchange() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"IGES exchange test folder failed");
    cad::SceneDocument source;
    require(source.addPrimitive(cad::Primitive::Box,{12,10,8},&error),"Could not create the first IGES source solid");
    require(source.addPrimitive(cad::Primitive::Cylinder,{3,15},&error),"Could not create the second IGES source solid");
    require(source.translate(source.parts().last().id,20,5,0,&error),"Could not position the second IGES source solid");
    const QString path=dir.filePath("two-solid-exchange.iges");
    require(source.exportIGES(path,&error),("IGES export failed: "+error.toStdString()).c_str());
    require(QFileInfo(path).size()>1000,"IGES writer created an empty or truncated exchange file");
    cad::SceneDocument imported;
    require(!imported.importIGES(dir.filePath("missing.iges"),&error)&&imported.parts().isEmpty(),
            "Missing IGES input changed the active scene");
    error.clear();require(imported.importIGES(path,&error),("IGES import failed: "+error.toStdString()).c_str());
    require(imported.parts().size()==2,"IGES exchange did not preserve both solid bodies");
    double totalVolume=0.0;for(const cad::Part& part:imported.parts()){
        cad::BodyDiagnostics diagnostics;require(imported.diagnoseBody(part.id,&diagnostics,&error)&&diagnostics.valid,
            "IGES exchange produced an invalid solid body");totalVolume+=diagnostics.volume;
    }
    closeTo(totalVolume,12*10*8+std::numbers::pi*9*15,1e-4);
    require(imported.undo()&&imported.parts().isEmpty(),"IGES import was not one atomic undo operation");
    require(imported.redo()&&imported.parts().size()==2,"IGES import could not be redone");
    cad::SceneDocument empty;require(!empty.exportIGES(dir.filePath("empty.iges"),&error),"An empty scene was exported as IGES");
}
void testObjMeshImport() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"OBJ mesh-import test folder failed");
    const QByteArray cube=R"OBJ(# 10 mm closed cube; coordinates are interpreted as millimeters
v 0 0 0
v 10 0 0
v 10 10 0
v 0 10 0
v 0 0 10
v 10 0 10
v 10 10 10
v 0 10 10
f 1 4 3 2
f 5 6 7 8
f 1 2 6 5
f 4 8 7 3
f 1 5 8 4
f -7 -6 -2 -3
)OBJ";
    const QString path=dir.filePath("closed-cube.obj");writeBytes(path,cube);
    cad::SceneDocument doc;
    require(!doc.importOBJ(path,0,&error)&&doc.parts().isEmpty(),"Invalid OBJ weld tolerance changed the scene");
    error.clear();cad::MeshImportSummary repairSummary;const bool imported=doc.importOBJ(path,0.01,&error,&repairSummary);
    require(imported,("Closed OBJ import failed: "+error.toStdString()).c_str());
    require(repairSummary.inputVertices==8&&repairSummary.outputVertices==8&&repairSummary.outputTriangles==12,
            "OBJ import returned inaccurate cleanup counts for shared vertices");
    require(doc.parts().size()==1&&doc.parts().first().name=="closed-cube","OBJ import did not create one file-named body");
    cad::BodyDiagnostics diagnostics;require(doc.diagnoseBody(doc.parts().first().id,&diagnostics,&error)&&diagnostics.valid,
            "Imported OBJ mesh did not form a valid faceted solid");
    closeTo(diagnostics.volume,1000,1e-6);require(diagnostics.faces==12,"OBJ quads were not triangulated into the expected faceted shell");
    QStringList sourceVertices;QString seamSplit;int seamSplitVertexCount=0;
    for(const QString& line:QString::fromUtf8(cube).split('\n')){
        const QStringList fields=line.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),Qt::SkipEmptyParts);
        if(fields.isEmpty())continue;
        if(fields.first()==QStringLiteral("v")){sourceVertices.append(fields.mid(1).join(' '));continue;}
        if(fields.first()!=QStringLiteral("f"))continue;
        QVector<int> faceIndices;
        for(int i=1;i<fields.size();++i){const int raw=fields[i].section('/',0,0).toInt();faceIndices.append(raw>0?raw-1:sourceVertices.size()+raw);}
        for(int corner=1;corner+1<faceIndices.size();++corner){
            const int triangleIndices[]={faceIndices[0],faceIndices[corner],faceIndices[corner+1]};
            const int first=seamSplitVertexCount+1;
            for(int index:triangleIndices){seamSplit+=QStringLiteral("v %1\n").arg(sourceVertices[index]);++seamSplitVertexCount;}
            seamSplit+=QStringLiteral("f %1 %2 %3\n").arg(first).arg(first+1).arg(first+2);
        }
    }
    const QString seamSplitPath=dir.filePath("closed-cube-seam-split.obj");writeBytes(seamSplitPath,seamSplit.toUtf8());
    cad::SceneDocument seamSplitDoc;
    cad::MeshImportSummary seamSplitSummary;
    require(seamSplitDoc.importOBJ(seamSplitPath,0.01,&error,&seamSplitSummary)&&seamSplitDoc.parts().size()==1,
            ("OBJ cleanup failed to weld per-facet vertices: "+error.toStdString()).c_str());
    require(seamSplitSummary.outputVertices==8&&seamSplitSummary.weldedVertices>0&&seamSplitSummary.outputTriangles==12,
            "OBJ import did not report per-facet vertex welding accurately");
    cad::BodyDiagnostics seamSplitDiagnostics;
    require(seamSplitDoc.diagnoseBody(seamSplitDoc.parts().first().id,&seamSplitDiagnostics,&error)&&seamSplitDiagnostics.valid,
            "Seam-split OBJ did not become a valid faceted solid after mesh repair");
    closeTo(seamSplitDiagnostics.volume,1000,1e-6);
    QStringList commentedLines=QString::fromUtf8(cube).split('\n');QByteArray commentedCube;
    for(QString line:commentedLines){
        if(line.startsWith("v ")||line.startsWith("f "))line+=" # exporter annotation";
        commentedCube+=line.toUtf8()+"\n";
    }
    const QString commentedPath=dir.filePath("closed-cube-comments.obj");writeBytes(commentedPath,commentedCube);
    cad::SceneDocument commentedDoc;
    require(commentedDoc.importOBJ(commentedPath,0.01,&error)&&commentedDoc.parts().size()==1,
            ("Valid OBJ inline comments were not ignored: "+error.toStdString()).c_str());
    QByteArray commentedBadIndex=cube;commentedBadIndex.replace("f 1 4 3 2","f 99 4 3 2 # invalid vertex before comment");
    const QString commentedBadPath=dir.filePath("bad-index-comments.obj");writeBytes(commentedBadPath,commentedBadIndex);
    require(!doc.importOBJ(commentedBadPath,0.01,&error)&&doc.parts().size()==1,
            "An invalid OBJ index before an inline comment was accepted or changed the scene");
    require(doc.undo()&&doc.parts().isEmpty()&&doc.redo()&&doc.parts().size()==1,"OBJ import was not one undoable operation");
    QByteArray open=cube;open.replace("f 1 4 3 2\n","");const QString openPath=dir.filePath("open-cube.obj");writeBytes(openPath,open);
    require(!doc.importOBJ(openPath,0.01,&error)&&doc.parts().size()==1,"Open OBJ mesh was accepted or partially changed the scene");
    cad::SceneDocument openSurfaceDoc;cad::MeshImportSummary openSurfaceSummary;
    require(openSurfaceDoc.importOBJ(openPath,0.01,&error,&openSurfaceSummary,false,true)&&
            openSurfaceDoc.parts().size()==1&&openSurfaceDoc.parts().first().isSurfaceBody&&
            openSurfaceDoc.parts().first().shape.ShapeType()==TopAbs_SHELL&&openSurfaceSummary.importedAsSurfaceBody,
            ("Open OBJ could not be retained as a faceted surface body: "+error.toStdString()).c_str());
    require(!openSurfaceDoc.importOBJ(openPath,0.01,&error,nullptr,true,true)&&openSurfaceDoc.parts().size()==1,
            "Conflicting mesh surface/hole-fill modes were accepted or changed the scene");
    const QString openSurfacePath=dir.filePath("open-obj-surface.cadscene");
    require(openSurfaceDoc.saveAs(openSurfacePath,&error),"Could not save the imported open OBJ surface body");
    cad::SceneDocument reopenedOpenSurface;
    require(reopenedOpenSurface.load(openSurfacePath,&error)&&reopenedOpenSurface.parts().size()==1&&
            reopenedOpenSurface.parts().first().isSurfaceBody&&
            reopenedOpenSurface.parts().first().shape.ShapeType()==TopAbs_SHELL,
            "Imported open OBJ surface body did not survive save/reopen");
    error.clear();cad::SceneDocument planarHoleRepairDoc;cad::MeshImportSummary planarHoleSummary;
    const bool planarHoleImport=planarHoleRepairDoc.importOBJ(openPath,0.01,&error,&planarHoleSummary,true);
    require(planarHoleImport&&planarHoleRepairDoc.parts().size()==1,
            ("A convex planar OBJ boundary was not repairable: "+error.toStdString()).c_str());
    cad::BodyDiagnostics repairedOpenDiagnostics;
    require(planarHoleRepairDoc.diagnoseBody(planarHoleRepairDoc.parts().first().id,&repairedOpenDiagnostics,&error)&&
            repairedOpenDiagnostics.valid&&planarHoleSummary.filledHoles==1&&planarHoleSummary.outputTriangles==12,
            "Planar OBJ hole repair did not create a valid solid or report the repair");
    closeTo(repairedOpenDiagnostics.volume,1000,1e-6);
    QByteArray malformed=cube;malformed.replace("f 1 4 3 2","f 0 4 3 2");const QString malformedPath=dir.filePath("bad-index.obj");writeBytes(malformedPath,malformed);
    require(!doc.importOBJ(malformedPath,0.01,&error)&&doc.parts().size()==1,"Malformed OBJ index was accepted or changed the scene");
    require(!doc.importOBJ(dir.filePath("missing.obj"),0.01,&error)&&doc.parts().size()==1,"Missing OBJ input changed the scene");
}
void testParametricFoundation() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Parametric test folder failed");
    cad::SceneDocument doc;
    require(!doc.addParametricRectangularExtrusion(0,20,5,&error),"Parametric rectangle accepted zero width");
    require(doc.parts().isEmpty()&&!doc.canUndo(),"Invalid parametric sketch changed the scene");
    require(doc.addParametricRectangularExtrusion(40,30,20,&error),"Could not create constrained rectangle extrusion");
    const QString id=doc.parts().first().id;
    require(doc.parts().first().hasParametricDefinition&&doc.sketchConstraints(id).size()==5&&doc.featureHistory(id).size()==2,
            "Parametric model omitted its fixed-origin sketch constraints or editable feature history");
    const auto parameters=doc.namedParameters(id);
    closeTo(parameters.value("width"),40);closeTo(parameters.value("height"),30);closeTo(parameters.value("depth"),20);
    closeTo(volume(doc.parts().first().shape),24000);
    const QString oldToken=doc.parts().first().geometryToken;
    require(doc.setNamedParameter(id,"width",60,&error),"Could not edit named sketch width");
    closeTo(volume(doc.parts().first().shape),36000);
    closeTo(doc.namedParameters(id).value("width"),60);
    require(doc.parts().first().geometryToken!=oldToken,"Parametric rebuild did not invalidate stale subshape references");
    require(!doc.setNamedParameter(id,"unknown",5,&error)&&!doc.setNamedParameter(id,"depth",0,&error),
            "Invalid named parameter edit was accepted");
    require(doc.undo(),"Parametric parameter edit did not create an undo step");closeTo(volume(doc.parts().first().shape),24000);
    require(doc.redo(),"Parametric parameter edit could not be redone");closeTo(volume(doc.parts().first().shape),36000);
    // No-op edits must retain the shape, geometry token, and undo/redo state.
    const auto unchangedShape=doc.parts().first().shape;
    const QString unchangedToken=doc.parts().first().geometryToken;
    const bool undoBeforeNoop=doc.canUndo();
    require(doc.setNamedParameter(id,"width",60,&error),"Unchanged named dimension was rejected");
    require(doc.setNamedParameters(id,{{"width",60},{"height",30},{"depth",20}},&error),
            "Unchanged batch of named dimensions was rejected");
    require(doc.parts().first().shape.IsSame(unchangedShape)&&
            doc.parts().first().geometryToken==unchangedToken&&doc.canUndo()==undoBeforeNoop,
            "No-op named parameter edit unexpectedly rebuilt geometry or changed undo state");
    require(!doc.setNamedParameters(id,{{"width",60},{"depth",0}},&error)&&
            doc.parts().first().shape.IsSame(unchangedShape)&&doc.parts().first().geometryToken==unchangedToken,
            "Invalid parameter in an otherwise unchanged batch bypassed validation");
    require(doc.undo(),"No-op parametric edit disrupted the previous undo step");
    closeTo(doc.namedParameters(id).value("width"),40);
    require(doc.redo(),"No-op parametric edit disrupted the previous redo step");
    closeTo(doc.namedParameters(id).value("width"),60);
    const QString path=dir.filePath("parametric.cadscene");
    require(doc.saveAs(path,&error),"Could not save parametric feature history");
    cad::SceneDocument loaded;require(loaded.load(path,&error),"Could not reload parametric feature history");
    require(loaded.parts().first().hasParametricDefinition&&loaded.sketchConstraints(id)==doc.sketchConstraints(id)&&
            loaded.featureHistory(id)==doc.featureHistory(id),"Parametric constraints or editable history did not persist");
    closeTo(loaded.namedParameters(id).value("width"),60);closeTo(volume(loaded.parts().first().shape),36000);
    QJsonObject root=QJsonDocument::fromJson(readBytes(path)).object();QJsonArray parts=root.value("parts").toArray();
    QJsonObject part=parts[0].toObject();QJsonObject definition=part.value("parametric_definition").toObject();
    QJsonObject named=definition.value("parameters").toObject();named.insert("width",61.0);definition.insert("parameters",named);
    part.insert("parametric_definition",definition);parts[0]=part;root.insert("parts",parts);
    const QString corrupt=dir.filePath("parametric-mismatch.cadscene");writeBytes(corrupt,QJsonDocument(root).toJson());
    require(!loaded.load(corrupt,&error)&&loaded.parts().size()==1&&loaded.namedParameters(id).value("width")==60,
            "Mismatched feature parameters were loaded or partially replaced the open scene");
    require(loaded.translate(id,10,0,0,&error),"Could not apply a direct edit to a parametric body");
    require(!loaded.parametricDefinitionCurrent(id)&&!loaded.setNamedParameter(id,"width",70,&error),
            "A stale feature history allowed edits that would overwrite later direct geometry changes");
    const QString stalePath=dir.filePath("parametric-stale.cadscene");
    require(loaded.saveAs(stalePath,&error),"Could not save a directly edited body with an archived parametric history");
    cad::SceneDocument staleLoaded;require(staleLoaded.load(stalePath,&error),"Stale parametric metadata prevented a valid scene from reopening");
    require(!staleLoaded.parametricDefinitionCurrent(id),"Reload incorrectly marked the old feature history current");
    closeTo(volume(staleLoaded.parts().first().shape),36000);

    cad::SceneDocument holeDoc;
    require(holeDoc.addParametricRectangularExtrusion(40,30,20,&error),"Could not create through-hole parametric base");
    const QString holeId=holeDoc.parts().first().id;
    require(!holeDoc.addParametricThroughHole(holeId,8,2,15,&error)&&holeDoc.featureHistory(holeId).size()==2,
            "An out-of-profile circle changed the parametric feature history");
    require(holeDoc.addParametricThroughHole(holeId,8,20,15,&error),"Could not add a constrained parametric through-hole");
    require(holeDoc.featureHistory(holeId).size()==4&&holeDoc.sketchConstraints(holeId).size()==8,
            "Through-hole sketch constraints or the short editable feature sequence were not recorded");
    const auto holeParams=holeDoc.namedParameters(holeId);
    closeTo(holeParams.value("hole_diameter"),8);closeTo(holeParams.value("hole_x"),20);closeTo(holeParams.value("hole_y"),15);
    closeTo(volume(holeDoc.parts().first().shape),24000-std::numbers::pi*16*20,1e-5);
    require(!holeDoc.setNamedParameter(holeId,"width",22,&error)&&holeDoc.namedParameters(holeId).value("width")==40,
            "A base-width edit that invalidates hole containment was accepted");
    const double originalHoleVolume=volume(holeDoc.parts().first().shape);
    require(!holeDoc.setNamedParameters(holeId,{{"width",8},{"hole_x",4}},&error)&&
            holeDoc.namedParameters(holeId).value("width")==40&&holeDoc.namedParameters(holeId).value("hole_x")==20&&
            std::abs(volume(holeDoc.parts().first().shape)-originalHoleVolume)<1e-6,
            "Invalid multi-parameter rebuild partially changed dimensions or geometry");
    require(holeDoc.setNamedParameters(holeId,{{"width",18},{"hole_x",9}},&error),
            "A coordinated width and hole-center edit could not rebuild as one operation");
    closeTo(holeDoc.namedParameters(holeId).value("width"),18);closeTo(holeDoc.namedParameters(holeId).value("hole_x"),9);
    require(holeDoc.undo()&&holeDoc.namedParameters(holeId).value("width")==40&&holeDoc.namedParameters(holeId).value("hole_x")==20,
            "Coordinated parametric edit was not undone atomically");
    require(holeDoc.redo()&&holeDoc.namedParameters(holeId).value("width")==18&&holeDoc.namedParameters(holeId).value("hole_x")==9,
            "Coordinated parametric edit was not redone atomically");
    require(holeDoc.setNamedParameters(holeId,holeDoc.namedParameters(holeId),&error),
            "An unchanged batch parameter set should be a successful no-op");
    require(holeDoc.setNamedParameters(holeId,{{"width",40},{"hole_x",20}},&error),
            "Could not restore the original dimensions after coordinated-edit checks");
    require(holeDoc.setNamedParameter(holeId,"hole_diameter",10,&error),"Could not edit the named through-hole diameter");
    closeTo(volume(holeDoc.parts().first().shape),24000-std::numbers::pi*25*20,1e-5);
    require(holeDoc.undo(),"Through-hole parameter edit was not undoable");
    closeTo(volume(holeDoc.parts().first().shape),24000-std::numbers::pi*16*20,1e-5);
    require(holeDoc.redo(),"Through-hole parameter edit could not be redone");
    const QString holePath=dir.filePath("parametric-through-hole.cadscene");
    require(holeDoc.saveAs(holePath,&error),"Could not save rectangle/extrusion/circle/cut history");
    cad::SceneDocument holeLoaded;require(holeLoaded.load(holePath,&error),"Could not reload the parametric through-hole history");
    require(holeLoaded.featureHistory(holeId)==holeDoc.featureHistory(holeId)&&
            holeLoaded.sketchConstraints(holeId)==holeDoc.sketchConstraints(holeId)&&
            holeLoaded.namedParameters(holeId)==holeDoc.namedParameters(holeId),
            "Through-hole sketch, dimensions, or feature sequence did not persist");
    closeTo(volume(holeLoaded.parts().first().shape),24000-std::numbers::pi*25*20,1e-5);
    // Moving only the saved hole center preserves the solid's bounding box and
    // volume. It must not grant an editable history for geometry at another X.
    QJsonObject movedHoleRoot=QJsonDocument::fromJson(readBytes(holePath)).object();
    QJsonArray movedHoleParts=movedHoleRoot.value("parts").toArray();
    QJsonObject movedHolePart=movedHoleParts[0].toObject();
    QJsonObject movedHoleDefinition=movedHolePart.value("parametric_definition").toObject();
    QJsonObject movedHoleParameters=movedHoleDefinition.value("parameters").toObject();
    movedHoleParameters.insert("hole_x",30.0);
    movedHoleDefinition.insert("parameters",movedHoleParameters);
    movedHolePart.insert("parametric_definition",movedHoleDefinition);
    movedHoleParts[0]=movedHolePart;movedHoleRoot.insert("parts",movedHoleParts);
    const QString movedHolePath=dir.filePath("parametric-hole-center-mismatch.cadscene");
    writeBytes(movedHolePath,QJsonDocument(movedHoleRoot).toJson());
    const TopoDS_Shape shapeBeforeFailedLoad=holeLoaded.parts().first().shape;
    const QString tokenBeforeFailedLoad=holeLoaded.parts().first().geometryToken;
    error.clear();
    require(!holeLoaded.load(movedHolePath,&error)&&
            error.contains("through-hole centers")&&
            holeLoaded.parametricDefinitionCurrent(holeId)&&
            holeLoaded.parts().first().shape.IsSame(shapeBeforeFailedLoad)&&
            holeLoaded.parts().first().geometryToken==tokenBeforeFailedLoad&&
            holeLoaded.namedParameters(holeId).value("hole_x")==20,
            "Saved through-hole center mismatch was accepted or changed the open scene");
    // A wider 12 mm-deep blind bore removes the same volume as the named
    // 10 mm-diameter through-hole. Mid-depth and planar probes all report
    // a void; the saved history must also match the cut through Z.
    const TopoDS_Shape blindBox=BRepPrimAPI_MakeBox(40,30,20).Shape();
    const TopoDS_Shape blindCutter=BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(20,15,-0.01),gp_Dir(0,0,1)),5.0*std::sqrt(20.0/12.0),12.01).Shape();
    BRepAlgoAPI_Cut blindCut(blindBox,blindCutter);blindCut.Build();
    require(blindCut.IsDone()&&!blindCut.HasErrors()&&BRepCheck_Analyzer(blindCut.Shape()).IsValid(),
            "Could not construct a valid blind-hole adversarial B-rep");
    closeTo(volume(blindCut.Shape()),volume(holeLoaded.parts().first().shape),1e-8);
    std::ostringstream blindBrep;BRepTools::Write(blindCut.Shape(),blindBrep);
    require(blindBrep.good(),"Failed to serialize blind-hole adversarial B-rep");
    QJsonObject blindRoot=QJsonDocument::fromJson(readBytes(holePath)).object();
    QJsonArray blindParts=blindRoot.value("parts").toArray();
    QJsonObject blindPart=blindParts[0].toObject();
    blindPart.insert("brep",QString::fromLatin1(QByteArray::fromStdString(blindBrep.str()).toBase64()));
    blindParts[0]=blindPart;blindRoot.insert("parts",blindParts);
    const QString blindForgery=dir.filePath("parametric-blind-hole-as-through.cadscene");
    writeBytes(blindForgery,QJsonDocument(blindRoot).toJson());
    error.clear();
    require(!holeLoaded.load(blindForgery,&error)&&error.contains("cut depths")&&
            holeLoaded.parametricDefinitionCurrent(holeId)&&
            holeLoaded.parts().first().shape.IsSame(shapeBeforeFailedLoad)&&
            holeLoaded.parts().first().geometryToken==tokenBeforeFailedLoad,
            "Saved blind hole was accepted as a through-hole or changed the open scene");
    require(holeDoc.setNamedParameter(holeId,"hole_x",10,&error),"Could not position the seed hole for a linear pattern");
    require(!holeDoc.addParametricLinearHolePattern(holeId,3,9,&error)&&holeDoc.featureHistory(holeId).size()==4,
            "Overlapping linear hole-pattern instances changed the feature history");
    require(!holeDoc.addParametricLinearHolePattern(holeId,1,10,&error),"A one-instance pattern was accepted");
    require(holeDoc.addParametricLinearHolePattern(holeId,3,10,&error),"Could not create a constrained linear through-hole pattern");
    require(holeDoc.featureHistory(holeId).size()==5&&holeDoc.sketchConstraints(holeId).size()==10,
            "Linear pattern constraints or ordered feature history were not recorded");
    closeTo(holeDoc.namedParameters(holeId).value("hole_pattern_count"),3);
    closeTo(holeDoc.namedParameters(holeId).value("hole_pattern_spacing"),10);
    closeTo(volume(holeDoc.parts().first().shape),24000-3*std::numbers::pi*25*20,1e-5);
    const TopoDS_Shape beforePatternRejection=holeDoc.parts().first().shape;
    const QString tokenBeforePatternRejection=holeDoc.parts().first().geometryToken;
    require(!holeDoc.setNamedParameters(holeId,{{"hole_pattern_spacing",16}},&error)&&
            holeDoc.parts().first().shape.IsSame(beforePatternRejection)&&
            holeDoc.parts().first().geometryToken==tokenBeforePatternRejection&&
            holeDoc.namedParameters(holeId).value("hole_pattern_spacing")==10,
            "Rejected out-of-profile pattern spacing changed geometry or dimensions");
    require(!holeDoc.setNamedParameters(holeId,{{"hole_pattern_count",2.5}},&error)&&
            holeDoc.namedParameters(holeId).value("hole_pattern_count")==3,
            "A fractional pattern count changed the saved parameters");
    require(holeDoc.undo()&&holeDoc.featureHistory(holeId).size()==4&&
            std::abs(volume(holeDoc.parts().first().shape)-(24000-std::numbers::pi*25*20))<1e-5,
            "Linear hole pattern did not undo as one feature operation");
    require(holeDoc.redo()&&holeDoc.featureHistory(holeId).size()==5,
            "Linear hole pattern could not be redone");
    const QString patternPath=dir.filePath("parametric-hole-pattern.cadscene");
    require(holeDoc.saveAs(patternPath,&error),"Could not save a parametric linear hole pattern");
    // This pattern has 10 mm diameter and exactly 10 mm spacing: tangent
    // bores must still reopen as an editable history.
    cad::SceneDocument patternLoaded;error.clear();require(patternLoaded.load(patternPath,&error),("Could not reload the parametric linear hole pattern: "+error.toStdString()).c_str());
    require(patternLoaded.featureHistory(holeId)==holeDoc.featureHistory(holeId)&&
            patternLoaded.sketchConstraints(holeId)==holeDoc.sketchConstraints(holeId)&&
            patternLoaded.namedParameters(holeId)==holeDoc.namedParameters(holeId),
            "Linear pattern parameters, constraints, or feature order did not persist");
    closeTo(volume(patternLoaded.parts().first().shape),24000-3*std::numbers::pi*25*20,1e-5);
    // An altered pattern-row Y remains in bounds and leaves the original shape
    // volume unchanged; every expected bore center must still be inspected.
    QJsonObject movedPatternRoot=QJsonDocument::fromJson(readBytes(patternPath)).object();
    QJsonArray movedPatternParts=movedPatternRoot.value("parts").toArray();
    QJsonObject movedPatternPart=movedPatternParts[0].toObject();
    QJsonObject movedPatternDef=movedPatternPart.value("parametric_definition").toObject();
    QJsonObject movedPatternParameters=movedPatternDef.value("parameters").toObject();
    movedPatternParameters.insert("hole_y",8.0);
    movedPatternDef.insert("parameters",movedPatternParameters);
    movedPatternPart.insert("parametric_definition",movedPatternDef);
    movedPatternParts[0]=movedPatternPart;movedPatternRoot.insert("parts",movedPatternParts);
    const QString movedPatternPath=dir.filePath("parametric-pattern-center-mismatch.cadscene");
    writeBytes(movedPatternPath,QJsonDocument(movedPatternRoot).toJson());
    error.clear();
    require(!patternLoaded.load(movedPatternPath,&error)&&
            error.contains("through-hole centers")&&
            patternLoaded.parametricDefinitionCurrent(holeId)&&
            patternLoaded.namedParameters(holeId).value("hole_y")==15,
            "Saved linear-pattern center mismatch was accepted or changed the open scene");
    // A volume-preserving forged history can shrink the pattern from three
    // 8 mm holes to two wider holes without changing its box or total volume.
    // The original center-only probes miss this; near-wall probes must reject it.
    cad::SceneDocument radiusDoc;
    require(radiusDoc.addParametricRectangularExtrusion(70,30,20,&error),"Could not create bore-radius load test base");
    const QString radiusId=radiusDoc.parts().first().id;
    require(radiusDoc.addParametricThroughHole(radiusId,8,10,15,&error)&&
            radiusDoc.addParametricLinearHolePattern(radiusId,3,14,&error),
            "Could not create the separated three-hole load test pattern");
    const QString radiusPath=dir.filePath("parametric-pattern-radius.cadscene");
    require(radiusDoc.saveAs(radiusPath,&error),"Could not save the bore-radius reference pattern");
    cad::SceneDocument radiusLoaded;error.clear();
    require(radiusLoaded.load(radiusPath,&error),"Valid separated pattern failed to load");
    QJsonObject radiusRoot=QJsonDocument::fromJson(readBytes(radiusPath)).object();
    QJsonArray radiusParts=radiusRoot.value("parts").toArray();
    QJsonObject radiusPart=radiusParts[0].toObject();
    QJsonObject radiusDef=radiusPart.value("parametric_definition").toObject();
    QJsonObject radiusParams=radiusDef.value("parameters").toObject();
    radiusParams.insert("hole_pattern_count",2.0);
    radiusParams.insert("hole_diameter",8.0*std::sqrt(1.5));
    radiusDef.insert("parameters",radiusParams);radiusPart.insert("parametric_definition",radiusDef);
    radiusParts[0]=radiusPart;radiusRoot.insert("parts",radiusParts);
    const QString radiusForgery=dir.filePath("parametric-pattern-radius-forgery.cadscene");
    writeBytes(radiusForgery,QJsonDocument(radiusRoot).toJson());
    error.clear();
    require(!radiusLoaded.load(radiusForgery,&error)&&error.contains("bore radii")&&
            radiusLoaded.parametricDefinitionCurrent(radiusId)&&
            radiusLoaded.namedParameters(radiusId).value("hole_pattern_count")==3&&
            radiusLoaded.namedParameters(radiusId).value("hole_diameter")==8,
            "Volume-preserving forged bore radii were loaded or changed the open scene");

    QJsonObject patternRoot=QJsonDocument::fromJson(readBytes(patternPath)).object();QJsonArray patternParts=patternRoot.value("parts").toArray();
    QJsonObject patternPart=patternParts[0].toObject();QJsonObject patternDef=patternPart.value("parametric_definition").toObject();
    QJsonObject patternParams=patternDef.value("parameters").toObject();patternParams.insert("hole_pattern_count",2.5);
    patternDef.insert("parameters",patternParams);patternPart.insert("parametric_definition",patternDef);patternParts[0]=patternPart;
    patternRoot.insert("parts",patternParts);const QString invalidPatternPath=dir.filePath("parametric-hole-pattern-invalid.cadscene");
    writeBytes(invalidPatternPath,QJsonDocument(patternRoot).toJson());
    require(!patternLoaded.load(invalidPatternPath,&error)&&patternLoaded.parts().size()==1&&
            patternLoaded.namedParameters(holeId).value("hole_pattern_count")==3,
            "Invalid saved linear-pattern dimensions replaced the open project");

    // High-count pattern construction and rebuild should remain atomic with
    // one compound-cutter Boolean, including the supported 100-hole limit.
    cad::SceneDocument largePatternDoc;
    require(largePatternDoc.addParametricRectangularExtrusion(1150,30,20,&error),
            "Could not create high-count pattern base");
    const QString largePatternId=largePatternDoc.parts().first().id;
    require(largePatternDoc.addParametricThroughHole(largePatternId,10,12,15,&error)&&
            largePatternDoc.addParametricLinearHolePattern(largePatternId,100,11,&error),
            "Could not construct a 100-hole parametric pattern");
    closeTo(volume(largePatternDoc.parts().first().shape),
            (1150*30-100*std::numbers::pi*25)*20,1e-4);
    require(largePatternDoc.setNamedParameters(largePatternId,{{"hole_pattern_count",99},{"hole_pattern_spacing",11.1}},&error),
            "Coordinated high-count pattern rebuild failed");
    closeTo(volume(largePatternDoc.parts().first().shape),
            (1150*30-99*std::numbers::pi*25)*20,1e-4);
    require(largePatternDoc.undo()&&
            largePatternDoc.namedParameters(largePatternId).value("hole_pattern_count")==100&&
            largePatternDoc.redo()&&
            largePatternDoc.namedParameters(largePatternId).value("hole_pattern_count")==99,
            "High-count pattern rebuild did not preserve atomic undo/redo");
    const QString largePatternPath=dir.filePath("parametric-pattern-high-count.cadscene");
    require(largePatternDoc.saveAs(largePatternPath,&error),"Could not save high-count pattern");
    cad::SceneDocument largeLoaded;error.clear();
    require(largeLoaded.load(largePatternPath,&error)&&
            largeLoaded.parametricDefinitionCurrent(largePatternId)&&
            largeLoaded.namedParameters(largePatternId)==largePatternDoc.namedParameters(largePatternId),
            "High-count pattern failed saved-history round trip");

    cad::SceneDocument yPatternDoc;
    require(yPatternDoc.addParametricRectangularExtrusion(40,60,20,&error),"Could not create Y-pattern parametric base");
    const QString yPatternId=yPatternDoc.parts().first().id;
    require(yPatternDoc.addParametricThroughHole(yPatternId,10,20,10,&error),"Could not add Y-pattern seed hole");
    require(!yPatternDoc.addParametricLinearHolePattern(yPatternId,4,9,QStringLiteral("Y"),&error),
            "An overlapping Y-direction pattern was accepted");
    require(yPatternDoc.addParametricLinearHolePattern(yPatternId,4,12,QStringLiteral("Y"),&error),
            "Could not create a Y-direction linear through-hole pattern");
    require(yPatternDoc.parts().first().holePatternAxis==QStringLiteral("Y")&&
            yPatternDoc.featureHistory(yPatternId).last()==QStringLiteral("Pattern: Linear through-hole array along Y")&&
            yPatternDoc.sketchConstraints(yPatternId).at(8)==QStringLiteral("Pattern: hole centers spaced by hole_pattern_spacing along Y"),
            "Y-direction pattern axis was not represented in its parametric definition");
    closeTo(volume(yPatternDoc.parts().first().shape),48000-4*std::numbers::pi*25*20,1e-5);
    require(!yPatternDoc.setNamedParameters(yPatternId,{{"height",50}},&error)&&
            yPatternDoc.namedParameters(yPatternId).value("height")==60,
            "A height edit that clips the final Y-pattern instance was accepted");
    require(yPatternDoc.setNamedParameters(yPatternId,{{"hole_pattern_count",3},{"hole_pattern_spacing",15}},&error),
            "A coordinated Y-pattern count and spacing rebuild failed");
    closeTo(volume(yPatternDoc.parts().first().shape),48000-3*std::numbers::pi*25*20,1e-5);
    require(yPatternDoc.undo()&&yPatternDoc.namedParameters(yPatternId).value("hole_pattern_count")==4&&
            yPatternDoc.parts().first().holePatternAxis==QStringLiteral("Y"),
            "Y-pattern parameter rebuild was not undoable as one operation");
    require(yPatternDoc.redo()&&yPatternDoc.namedParameters(yPatternId).value("hole_pattern_count")==3,
            "Y-pattern parameter rebuild could not be redone");
    const QString yPatternPath=dir.filePath("parametric-hole-pattern-y.cadscene");
    require(yPatternDoc.saveAs(yPatternPath,&error),"Could not save a Y-direction parametric pattern");
    cad::SceneDocument yPatternLoaded;
    require(yPatternLoaded.load(yPatternPath,&error)&&yPatternLoaded.parts().first().holePatternAxis==QStringLiteral("Y")&&
            yPatternLoaded.featureHistory(yPatternId)==yPatternDoc.featureHistory(yPatternId),
            "Y-direction pattern axis or history did not persist");
    // Keep the first Y-pattern bore unchanged but move later expected centers
    // into solid material. This detects validators that only check the seed.
    QJsonObject movedYRoot=QJsonDocument::fromJson(readBytes(yPatternPath)).object();
    QJsonArray movedYParts=movedYRoot.value("parts").toArray();
    QJsonObject movedYPart=movedYParts[0].toObject();
    QJsonObject movedYDef=movedYPart.value("parametric_definition").toObject();
    QJsonObject movedYParameters=movedYDef.value("parameters").toObject();
    movedYParameters.insert("hole_pattern_spacing",11.0);
    movedYDef.insert("parameters",movedYParameters);
    movedYPart.insert("parametric_definition",movedYDef);
    movedYParts[0]=movedYPart;movedYRoot.insert("parts",movedYParts);
    const QString movedYPath=dir.filePath("parametric-y-pattern-later-center-mismatch.cadscene");
    writeBytes(movedYPath,QJsonDocument(movedYRoot).toJson());
    error.clear();
    require(!yPatternLoaded.load(movedYPath,&error)&&
            error.contains("through-hole centers")&&
            yPatternLoaded.parametricDefinitionCurrent(yPatternId)&&
            yPatternLoaded.namedParameters(yPatternId).value("hole_pattern_spacing")==15,
            "Saved Y-pattern center mismatch was accepted or changed the open scene");

    QJsonObject legacyPatternRoot=QJsonDocument::fromJson(readBytes(patternPath)).object();
    legacyPatternRoot.insert("schema","cadworkbench.scene.v18");
    QJsonArray legacyPatternParts=legacyPatternRoot.value("parts").toArray();
    QJsonObject legacyPatternPart=legacyPatternParts[0].toObject();
    QJsonObject legacyPatternDefinition=legacyPatternPart.value("parametric_definition").toObject();
    legacyPatternDefinition.remove("pattern_axis");legacyPatternPart.insert("parametric_definition",legacyPatternDefinition);
    legacyPatternParts[0]=legacyPatternPart;legacyPatternRoot.insert("parts",legacyPatternParts);
    const QString legacyPatternPath=dir.filePath("parametric-hole-pattern-v18.cadscene");
    writeBytes(legacyPatternPath,QJsonDocument(legacyPatternRoot).toJson());
    cad::SceneDocument legacyPatternLoaded;
    require(legacyPatternLoaded.load(legacyPatternPath,&error)&&
            legacyPatternLoaded.parts().first().holePatternAxis==QStringLiteral("X"),
            "A schema-v18 X-direction pattern did not migrate with its original axis");

    cad::SceneDocument offsetDoc;
    require(offsetDoc.addParametricRectangularExtrusionAt(-20,5,40,30,10,&error),"Could not create an offset fully constrained rectangle");
    const QString offsetId=offsetDoc.parts().first().id;
    const auto offsetParams=offsetDoc.namedParameters(offsetId);
    closeTo(offsetParams.value("origin_x"),-20);closeTo(offsetParams.value("origin_y"),5);
    require(offsetDoc.sketchConstraints(offsetId).size()==6&&offsetDoc.featureHistory(offsetId).size()==2,
            "Offset sketch did not record both origin dimensions and the base feature history");
    require(offsetDoc.addParametricThroughHole(offsetId,6,0,20,&error),"Offset sketch rejected an in-profile parametric through-hole");
    require(!offsetDoc.addParametricThroughHole(offsetId,6,18,20,&error),"Offset sketch accepted a hole crossing its right boundary");
    require(offsetDoc.setNamedParameters(offsetId,{{"origin_x",-30},{"hole_x",-15}},&error),
            "Coordinated offset-origin and dependent hole edit failed");
    closeTo(offsetDoc.namedParameters(offsetId).value("origin_x"),-30);closeTo(offsetDoc.namedParameters(offsetId).value("hole_x"),-15);
    Bnd_Box offsetBounds;BRepBndLib::Add(offsetDoc.parts().first().shape,offsetBounds);double ox0=0,oy0=0,oz0=0,ox1=0,oy1=0,oz1=0;offsetBounds.Get(ox0,oy0,oz0,ox1,oy1,oz1);
    closeTo(ox0,-30);closeTo(oy0,5);closeTo(ox1,10);closeTo(oy1,35);
    require(offsetDoc.undo()&&offsetDoc.namedParameters(offsetId).value("origin_x")==-20&&offsetDoc.namedParameters(offsetId).value("hole_x")==0,
            "Offset sketch batch edit did not undo both dimensions");
    require(offsetDoc.redo()&&offsetDoc.namedParameters(offsetId).value("origin_x")==-30&&offsetDoc.namedParameters(offsetId).value("hole_x")==-15,
            "Offset sketch batch edit did not redo both dimensions");
    const QString offsetPath=dir.filePath("parametric-offset.cadscene");require(offsetDoc.saveAs(offsetPath,&error),"Could not save offset parametric history");
    cad::SceneDocument offsetLoaded;require(offsetLoaded.load(offsetPath,&error),"Could not reload offset parametric history");
    require(offsetLoaded.namedParameters(offsetId)==offsetDoc.namedParameters(offsetId)&&
            offsetLoaded.featureHistory(offsetId)==offsetDoc.featureHistory(offsetId)&&
            offsetLoaded.sketchConstraints(offsetId)==offsetDoc.sketchConstraints(offsetId),
            "Offset sketch constraints, parameters, or feature history did not persist");
    // Valid archived offset histories remain readable after later direct edits.
    QJsonObject archivedRoot=QJsonDocument::fromJson(readBytes(offsetPath)).object();
    QJsonArray archivedParts=archivedRoot.value("parts").toArray();
    QJsonObject archivedPart=archivedParts[0].toObject();
    QJsonObject archivedDefinition=archivedPart.value("parametric_definition").toObject();
    archivedDefinition.insert("geometry_token",QStringLiteral("stale-archived-history"));
    archivedPart.insert("parametric_definition",archivedDefinition);
    archivedParts[0]=archivedPart;archivedRoot.insert("parts",archivedParts);
    const QString archivedPath=dir.filePath("parametric-offset-stale-valid.cadscene");
    writeBytes(archivedPath,QJsonDocument(archivedRoot).toJson());
    cad::SceneDocument archivedDoc;error.clear();
    require(archivedDoc.load(archivedPath,&error)&&
            !archivedDoc.parametricDefinitionCurrent(offsetId)&&
            archivedDoc.namedParameters(offsetId)==offsetDoc.namedParameters(offsetId)&&
            !archivedDoc.setNamedParameter(offsetId,"width",45,&error),
            "Valid archived offset sketch was rejected or incorrectly made editable");
    // A stale archived history must not bypass the sketch-boundary rules at load.
    // Both individual coordinates are valid, but their sum with the profile
    // dimension exceeds the maximum supported sketch coordinate.
    for(const QString& coordinate:{QStringLiteral("origin_x"),QStringLiteral("origin_y")}){
        QJsonObject savedRoot=QJsonDocument::fromJson(readBytes(offsetPath)).object();
        QJsonArray savedParts=savedRoot.value("parts").toArray();
        QJsonObject savedPart=savedParts[0].toObject();
        QJsonObject savedDefinition=savedPart.value("parametric_definition").toObject();
        QJsonObject savedParameters=savedDefinition.value("parameters").toObject();
        savedParameters.insert(coordinate,999990.0);
        savedDefinition.insert("parameters",savedParameters);
        savedDefinition.insert("geometry_token",QStringLiteral("stale-archived-history"));
        savedPart.insert("parametric_definition",savedDefinition);
        savedParts[0]=savedPart;savedRoot.insert("parts",savedParts);
        const QString invalidPath=dir.filePath(QStringLiteral("parametric-offset-out-of-bounds-")+coordinate+QStringLiteral(".cadscene"));
        writeBytes(invalidPath,QJsonDocument(savedRoot).toJson());
        error.clear();
        require(!offsetLoaded.load(invalidPath,&error)&&
                error.contains("supported coordinate range")&&
                offsetLoaded.namedParameters(offsetId)==offsetDoc.namedParameters(offsetId)&&
                offsetLoaded.parametricDefinitionCurrent(offsetId),
                "An out-of-bounds archived offset sketch loaded or changed the open scene");
    }
}
void testSheetMetalFlatPattern() {
    QString error;cad::SheetMetalRule rule{2.0,2.0,0.33};cad::SheetMetalFlatPattern result;
    QVector<double> numericValues{7.0};
    require(cad::parseSheetMetalNumberList("100, 70 ,60",&numericValues,&error)&&
            numericValues==QVector<double>({100.0,70.0,60.0}),
            "Valid comma-separated sheet-metal dimensions were not parsed correctly");
    const QVector<double> validParsedValues=numericValues;
    QVector<double> scientificValues;
    require(cad::parseSheetMetalNumberList("1e2, +7.0 , -4",&scientificValues,&error)&&
            scientificValues==QVector<double>({100.0,7.0,-4.0}),
            "Strict sheet-metal parsing unexpectedly rejected signed or scientific numbers");
    for(const QString& malformed:{QStringLiteral("100,,70,60"),QStringLiteral(",100,70"),
                                  QStringLiteral("100,70,"),QStringLiteral("100,  ,70"),
                                  QStringLiteral("100,NaN,70"),QStringLiteral("100,not-a-number,70")}){
        require(!cad::parseSheetMetalNumberList(malformed,&numericValues,&error)&&
                numericValues==validParsedValues&&!error.isEmpty(),
                "Malformed numeric list was accepted or partially modified the parsed values");
    }
    require(!cad::parseSheetMetalNumberList("100,70",nullptr,&error),
            "Null sheet-metal numeric list output was accepted");
    require(cad::calculateOneBendFlatPattern(100,50,25,90,rule,&result,&error),"Valid one-bend flat pattern was rejected");
    const double allowance=(std::numbers::pi/2.0)*(2.0+0.33*2.0);
    const double deduction=2.0*(2.0+2.0)-allowance;
    closeTo(result.bendAllowanceMm,allowance);closeTo(result.bendDeductionMm,deduction);closeTo(result.flatLengthMm,150-deduction);
    require(result.firstTangentMm>0&&result.firstTangentMm<result.flatLengthMm,"Bend line lies outside the developed blank");
    QByteArray svg;require(cad::buildOneBendFlatPatternSvg(100,50,25,90,rule,&svg,&result,&error),"Could not build flat-pattern SVG");
    require(svg.contains("single-bend-flat-pattern")&&svg.contains("id=\"blank-outline\"")&&
            svg.contains("id=\"bend-line\"")&&svg.contains("data-thickness-mm=\"2\"")&&
            svg.contains("FLAT PATTERN - ONE BEND"),"Flat-pattern SVG omitted geometry or the rule metadata");
    // Physical SVG dimensions must not round a supported thin blank to 0 mm.
    QByteArray narrowSvg;
    require(cad::buildOneBendFlatPatternSvg(100,50,0.00002,90,rule,&narrowSvg,nullptr,&error),
            "Supported narrow sheet width was rejected by SVG generation");
    require(narrowSvg.contains("height=\"0.000020mm\"")&&
            narrowSvg.contains("viewBox=\"0 0 ")&&narrowSvg.contains(" 0.000020\""),
            "SVG physical sheet height was rounded to zero or mismatched with its viewBox");
    require(narrowSvg.contains("width 2e-05 mm")&&!narrowSvg.contains("width 0.000 mm"),
            "Small positive sheet width was misreported as zero in human-readable SVG notes");
    // Rule metadata must round-trip, even when supported values are smaller
    // than the three/four decimal places previously used by the SVG writer.
    const cad::SheetMetalRule smallRule{0.00002,1e-10,1e-10};
    QByteArray preciseRuleSvg;
    const double exactAngle=37.123456789;
    require(cad::buildOneBendFlatPatternSvg(100,50,25,exactAngle,smallRule,
                &preciseRuleSvg,nullptr,&error),
            "Could not build a one-bend SVG with very small valid rule values");
    QXmlStreamReader ruleXml(preciseRuleSvg);
    while(!ruleXml.atEnd()&&!ruleXml.isStartElement())ruleXml.readNext();
    require(ruleXml.isStartElement()&&ruleXml.name()==QStringLiteral("svg"),
            "Generated flat-pattern SVG has no document root");
    const auto attributes=ruleXml.attributes();
    bool angleOk=false,thicknessOk=false,radiusOk=false,kOk=false;
    const double storedAngle=attributes.value(QStringLiteral("data-angle-degrees")).toString().toDouble(&angleOk);
    const double storedThickness=attributes.value(QStringLiteral("data-thickness-mm")).toString().toDouble(&thicknessOk);
    const double storedRadius=attributes.value(QStringLiteral("data-inside-radius-mm")).toString().toDouble(&radiusOk);
    const double storedK=attributes.value(QStringLiteral("data-k-factor")).toString().toDouble(&kOk);
    require(angleOk&&thicknessOk&&radiusOk&&kOk&&storedAngle==exactAngle&&
            storedThickness==smallRule.thicknessMm&&storedRadius==smallRule.insideBendRadiusMm&&
            storedK==smallRule.kFactor,
            "SVG rule metadata was rounded or failed to round-trip its finite input values");
    require(preciseRuleSvg.contains("t=2e-05 mm | R=1e-10 mm | K=1e-10")&&
            !preciseRuleSvg.contains("t=0.000 mm")&&!preciseRuleSvg.contains("R=0.000 mm"),
            "Human-readable SVG rule notes rounded valid positive dimensions to zero");
    require(!cad::calculateOneBendFlatPattern(100,50,25,0,rule,&result,&error),"Invalid bend angle was accepted");
    require(!cad::calculateOneBendFlatPattern(1,1,25,90,rule,&result,&error),"Flange lengths shorter than bend setback were accepted");
    cad::SheetMetalRule invalid=rule;invalid.kFactor=1.1;
    require(!cad::calculateOneBendFlatPattern(100,50,25,90,invalid,&result,&error),"Out-of-range K-factor was accepted");
    QTemporaryDir dir;require(dir.isValid(),"Sheet-metal export test folder failed");const QString path=dir.filePath("flat-pattern.svg");
    require(cad::exportOneBendFlatPatternSvg(100,50,25,90,rule,path,&result,&error),"Flat-pattern SVG file export failed");
    require(readBytes(path).contains("data-document-type=\"single-bend-flat-pattern\""),"Saved flat-pattern SVG differs from generated output");
    // A valid mathematical tangent must not round onto a rectangle edge in SVG.
    const cad::SheetMetalRule zeroAllowanceRule{2.0,0.0,0.0};
    cad::SheetMetalFlatPattern tinyClearance;
    require(cad::calculateOneBendFlatPattern(2.0+1e-8,30.0,25.0,90.0,
                zeroAllowanceRule,&tinyClearance,&error)&&tinyClearance.firstTangentMm>0.0,
            "Could not prepare a small but positive one-bend edge clearance");
    const QByteArray savedSvg=svg;
    require(!cad::buildOneBendFlatPatternSvg(2.0+1e-8,30.0,25.0,90.0,
                zeroAllowanceRule,&svg,nullptr,&error)&&error.contains("SVG precision")&&svg==savedSvg,
            "SVG bend line rounded onto the left edge or changed the output buffer");
    require(!cad::buildOneBendFlatPatternSvg(30.0,2.0+1e-8,25.0,90.0,
                zeroAllowanceRule,&svg,nullptr,&error)&&error.contains("SVG precision")&&svg==savedSvg,
            "SVG bend line rounded onto the right edge or changed the output buffer");
    const QByteArray previouslySavedSvg=readBytes(path);
    require(!cad::exportOneBendFlatPatternSvg(2.0+1e-8,30.0,25.0,90.0,
                zeroAllowanceRule,path,nullptr,&error)&&readBytes(path)==previouslySavedSvg,
            "Rejected SVG edge collision overwrote an existing good export");
    require(cad::buildOneBendFlatPatternSvg(2.0+0.000010,30.0,25.0,90.0,
                zeroAllowanceRule,&svg,nullptr,&error)&&svg.contains("d=\"M0.000010 0V25.000000\""),
            "Resolvable bend-line clearance was rejected or lost in SVG");
    svg=savedSvg;

    const QVector<double> flanges{100,70,60};
    const QVector<cad::SheetMetalBend> bends{{90,2.0,0.33},{90,3.0,0.40}};
    cad::MultiBendFlatPattern multi;
    require(cad::calculateMultiBendFlatPattern(flanges,25,2,bends,&multi,&error),"Valid two-bend flat pattern was rejected");
    const double allowanceA=std::numbers::pi/2.0*(2.0+0.33*2.0),allowanceB=std::numbers::pi/2.0*(3.0+0.40*2.0);
    const double deductionA=8.0-allowanceA,deductionB=10.0-allowanceB;
    closeTo(multi.bendAllowancesMm[0],allowanceA);closeTo(multi.bendAllowancesMm[1],allowanceB);
    closeTo(multi.bendDeductionsMm[0],deductionA);closeTo(multi.bendDeductionsMm[1],deductionB);
    closeTo(multi.flatLengthMm,230.0-deductionA-deductionB);
    closeTo(multi.bendLinePositionsMm[0],100.0-4.0+allowanceA/2.0);
    closeTo(multi.bendLinePositionsMm[1],multi.bendLinePositionsMm[0]+70.0-4.0-5.0+allowanceA/2.0+allowanceB/2.0);
    // Shop limits are explicit project inputs, not implicit material standards.
    require(cad::validateMultiBendShopLimits(flanges,25,2,bends,{},&error),
            "Default-disabled shop checks rejected a valid development");
    require(cad::validateMultiBendShopLimits(flanges,25,2,bends,{1.0,54.5},&error),
            "Valid radius ratio and straight-flange shop limits were rejected");
    require(!cad::validateMultiBendShopLimits(flanges,25,2,bends,{1.01,0.0},&error)&&
            error.contains("Bend 1")&&error.contains("radius"),
            "Shop radius/thickness check failed to identify bend 1");
    require(!cad::validateMultiBendShopLimits(flanges,25,2,bends,{0.0,55.5},&error)&&
            error.contains("Flange 3")&&error.contains("straight"),
            "Shop straight-flange check failed to identify flange 3");
    require(cad::validateMultiBendShopLimits({100,15,60},25,2,bends,{0.0,5.5},&error),
            "The exact middle flange's straight length should meet the limit");
    require(!cad::validateMultiBendShopLimits({100,15,60},25,2,bends,{0.0,6.5},&error)&&
            error.contains("Flange 2"),"Shop rule did not check both setbacks of a middle flange");
    require(!cad::validateMultiBendShopLimits(flanges,25,2,bends,{-1.0,0.0},&error)&&
            error.contains("finite and nonnegative"),"Negative shop rule was accepted");
    require(!cad::validateMultiBendShopLimits(flanges,25,2,bends,
                {std::numeric_limits<double>::infinity(),0.0},&error),
            "Non-finite shop rule was accepted");
    require(!cad::validateMultiBendShopLimits({1,1,1},25,2,bends,{},&error)&&
            error.contains("short"),"Shop validator skipped base flat-pattern feasibility");
    QByteArray dxf;require(cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,&dxf,&multi,&error),"Could not build two-bend DXF");
    require(dxf.startsWith("0\nSECTION\n2\nHEADER")&&dxf.contains("$ACADVER\n1\nAC1015")&&
            dxf.contains("$INSUNITS\n70\n4")&&dxf.contains("2\nCONTINUOUS\n")&&
            dxf.contains("2\nOUTLINE")&&dxf.contains("2\nBEND")&&dxf.contains("0\nEOF\n")&&
            dxf.count("0\nLINE\n8\nBEND\n")==2&&dxf.count("0\nLINE\n8\nOUTLINE\n")==4,
            "Multi-bend DXF omitted millimeter units, outline, bend lines, or EOF marker");
    const QString multiPath=dir.filePath("multi-bend.dxf");
    require(cad::exportMultiBendFlatPatternDxf(flanges,25,2,bends,multiPath,&multi,&error),"Multi-bend DXF file export failed");
    require(readBytes(multiPath)==dxf,"Saved multi-bend DXF differs from generated output");
    // The calculator can retain positive sub-micron clearances that the
    // six-decimal DXF writer cannot distinguish from a coincident station.
    const QVector<cad::SheetMetalBend> zeroAllowanceBends{{90,0,0},{90,0,0}};
    const QVector<double> closeStations{30.0,4.0+1e-8,30.0};
    cad::MultiBendFlatPattern closePattern;
    require(cad::calculateMultiBendFlatPattern(closeStations,25,2,zeroAllowanceBends,&closePattern,&error)&&
            closePattern.bendLinePositionsMm[1]>closePattern.bendLinePositionsMm[0],
            "Could not prepare distinct but DXF-unresolvable bend stations");
    const QByteArray savedDxf=dxf;
    require(!cad::buildMultiBendFlatPatternDxf(closeStations,25,2,zeroAllowanceBends,&dxf,nullptr,&error)&&
            error.contains("DXF precision")&&dxf==savedDxf,
            "Coincident serialized bend stations were accepted or changed the DXF buffer");
    require(!cad::exportMultiBendFlatPatternDxf(closeStations,25,2,zeroAllowanceBends,multiPath,nullptr,&error)&&
            readBytes(multiPath)==savedDxf,
            "Rejected bend stations overwrote a previously valid DXF file");
    require(!cad::buildMultiBendFlatPatternDxf({2.0+1e-8,14.0,30.0},25,2,
                zeroAllowanceBends,&dxf,nullptr,&error)&&error.contains("DXF precision"),
            "A bend center rounding onto the left blank edge was accepted");
    require(!cad::buildMultiBendFlatPatternDxf({30.0,14.0,2.0+1e-8},25,2,
                zeroAllowanceBends,&dxf,nullptr,&error)&&error.contains("DXF precision"),
            "A bend center rounding onto the right blank edge was accepted");
    require(cad::buildMultiBendFlatPatternDxf({30.0,4.0+0.000010,30.0},25,2,
                zeroAllowanceBends,&dxf,nullptr,&error)&&
            dxf.contains("10\n28.000000\n20\n")&&dxf.contains("10\n28.000010\n20\n"),
            "Distinct bend stations that survive DXF precision were rejected or collapsed");
    dxf=savedDxf;
    const cad::SheetMetalRelief relief{4.0,3.0};QByteArray relievedDxf;
    require(cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,relief,&relievedDxf,&multi,&error),
            "Valid rectangular bend-end reliefs were rejected");
    require(relievedDxf.count("0\nLINE\n8\nOUTLINE\n")==20&&relievedDxf.count("0\nLINE\n8\nBEND\n")==2,
            "Relieved DXF omitted the end notches or altered its bend lines");
    for(double x:multi.bendLinePositionsMm){
        const QByteArray xCoordinate=QString::number(x,'f',6).toUtf8();
        const QByteArray bendEntity="0\nLINE\n8\nBEND\n10\n"+xCoordinate+
            "\n20\n3.000000\n30\n0.0\n11\n"+xCoordinate+
            "\n21\n22.000000\n31\n0.0\n";
        require(relievedDxf.contains(bendEntity),
                "Bend line must stop at the inner edges of both end reliefs");
    }
    require(!cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,cad::SheetMetalRelief{0,3},&relievedDxf,&multi,&error),
            "A partial relief specification was accepted");
    require(!cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,cad::SheetMetalRelief{100,3},&relievedDxf,&multi,&error),
            "Relief notches extending beyond the blank were accepted");
    const QString relievedPath=dir.filePath("multi-bend-relieved.dxf");
    require(cad::exportMultiBendFlatPatternDxf(flanges,25,2,bends,relief,relievedPath,&multi,&error),
            "Could not export a multi-bend flat pattern with end reliefs");
    require(readBytes(relievedPath)==relievedDxf,"Saved relieved DXF differs from generated relief geometry");
    // A remaining web smaller than the six-decimal DXF resolution must not
    // collapse to a zero-length BEND entity after coordinate serialization.
    const QByteArray validRelievedDxf=relievedDxf;
    require(!cad::buildMultiBendFlatPatternDxf(flanges,10,2,bends,
                cad::SheetMetalRelief{4.0,4.9999999},&relievedDxf,nullptr,&error),
            "Relief with a bend line rounded to zero length was accepted");
    require(error.contains("DXF precision")&&relievedDxf==validRelievedDxf,
            "Invalid near-half-width relief changed the output or reported the wrong error");
    require(!cad::exportMultiBendFlatPatternDxf(flanges,10,2,bends,
                cad::SheetMetalRelief{4.0,4.9999999},relievedPath,nullptr,&error),
            "Near-half-width relief was accepted by file export");
    require(readBytes(relievedPath)==validRelievedDxf,
            "Rejected relief overwrote a previously valid DXF file");
    require(cad::buildMultiBendFlatPatternDxf(flanges,10,2,bends,
                cad::SheetMetalRelief{4.0,4.9999900},&relievedDxf,nullptr,&error),
            "Valid thin remaining web was rejected after the DXF rounding guard");
    require(relievedDxf.contains("\n20\n4.999990\n")&&relievedDxf.contains("\n21\n5.000010\n"),
            "Valid thin remaining web was not preserved in DXF bend-line endpoints");
    // Relief gaps that are positive in memory must remain nonzero after
    // six-decimal coordinate serialization, including edge-to-notch gaps.
    const QByteArray priorDxf=relievedDxf;
    const double nearTouchingWidth=multi.bendLinePositionsMm[1]-multi.bendLinePositionsMm[0]-1e-8;
    require(!cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,
                cad::SheetMetalRelief{nearTouchingWidth,3.0},&relievedDxf,nullptr,&error),
            "Almost touching relief notches produced a zero-length OUTLINE entity");
    require(error.contains("DXF precision")&&relievedDxf==priorDxf,
            "Near-touching reliefs overwrote the buffer or reported the wrong error");
    require(!cad::exportMultiBendFlatPatternDxf(flanges,25,2,bends,
                cad::SheetMetalRelief{nearTouchingWidth,3.0},relievedPath,nullptr,&error),
            "Near-touching relief notches were accepted by file export");
    require(readBytes(relievedPath)==validRelievedDxf,
            "Rejected near-touching reliefs overwrote the saved DXF");
    const QVector<double> shortFirstFlange{20.0,100.0,100.0};
    cad::MultiBendFlatPattern nearEdgePattern;
    require(cad::calculateMultiBendFlatPattern(shortFirstFlange,25,2,bends,&nearEdgePattern,&error),
            "Could not prepare near-edge relief fixture");
    const double nearEdgeWidth=2.0*nearEdgePattern.bendLinePositionsMm.first()-1e-8;
    require(!cad::buildMultiBendFlatPatternDxf(shortFirstFlange,25,2,bends,
                cad::SheetMetalRelief{nearEdgeWidth,3.0},&relievedDxf,nullptr,&error),
            "Relief almost touching the blank edge produced a zero-length OUTLINE entity");
    require(error.contains("DXF precision")&&relievedDxf==priorDxf,
            "Near-edge relief rejection changed the output or reported the wrong error");
    const QVector<double> shortLastFlange{100.0,100.0,20.0};
    cad::MultiBendFlatPattern rightEdgePattern;
    require(cad::calculateMultiBendFlatPattern(shortLastFlange,25,2,bends,&rightEdgePattern,&error),
            "Could not prepare right-edge relief fixture");
    const double nearRightWidth=2.0*(rightEdgePattern.flatLengthMm-rightEdgePattern.bendLinePositionsMm.last())-1e-8;
    require(!cad::buildMultiBendFlatPatternDxf(shortLastFlange,25,2,bends,
                cad::SheetMetalRelief{nearRightWidth,3.0},&relievedDxf,nullptr,&error),
            "Relief almost touching the far blank edge produced a zero-length OUTLINE entity");
    require(error.contains("DXF precision")&&relievedDxf==priorDxf,
            "Far-edge relief rejection changed the output or reported the wrong error");
    require(cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,
                cad::SheetMetalRelief{nearTouchingWidth-0.000010,3.0},&relievedDxf,nullptr,&error),
            "Relief notches with a resolvable gap were rejected");
    const double validWidth=nearTouchingWidth-0.000010;
    const QByteArray gapStart=QString::number(multi.bendLinePositionsMm[0]+validWidth/2.0,'f',6).toUtf8();
    const QByteArray gapEnd=QString::number(multi.bendLinePositionsMm[1]-validWidth/2.0,'f',6).toUtf8();
    const QByteArray gapEntity="0\nLINE\n8\nOUTLINE\n10\n"+gapStart+
        "\n20\n0.000000\n30\n0.0\n11\n"+gapEnd+"\n21\n0.000000\n31\n0.0\n";
    require(gapStart!=gapEnd&&relievedDxf.contains(gapEntity)&&
            relievedDxf.count("0\nLINE\n8\nOUTLINE\n")==20,
            "Resolvable gap between relief notches was lost in the exported outline");
    require(!cad::calculateMultiBendFlatPattern({100,70},25,2,bends,&multi,&error),"Mismatched flange and bend counts were accepted");
    require(!cad::calculateMultiBendFlatPattern({100,8,60},25,2,bends,&multi,&error),"An intermediate flange shorter than adjacent setbacks was accepted");
    require(!cad::calculateMultiBendFlatPattern(flanges,25,2,{{14,2,0.33},{90,2,0.33}},&multi,&error),"An invalid multi-bend angle was accepted");
    require(!cad::calculateMultiBendFlatPattern(flanges,25,2,{{90,2,0.33},{90,2,1.1}},&multi,&error),"An invalid per-bend K-factor was accepted");
    require(!cad::calculateMultiBendFlatPattern(flanges,25,2,{{90,2,0.33}},&multi,&error),"A one-bend request was accepted by the multi-bend operation");
    require(!cad::calculateMultiBendFlatPattern(flanges,25,2,bends,nullptr,&error),"A null multi-bend output record was accepted");
    require(!cad::buildMultiBendFlatPatternDxf(flanges,25,2,bends,nullptr,nullptr,&error),"A null multi-bend DXF buffer was accepted");
}

void testFoldedSheetMetal() {
    QString error;QTemporaryDir directory;require(directory.isValid(),"Folded sheet-metal temp folder unavailable");
    const QVector<double> flanges{40.0,30.0};
    const QVector<cad::FoldedSheetBend> bends{{90.0,2.0}};
    QVector<QPointF> outline{{-9,-9}};
    require(cad::buildFoldedSheetOutline(flanges,25.0,1.0,bends,&outline,&error)&&outline.size()>30,
            "Could not develop a radiused one-bend cross-section");
    cad::SceneDocument doc;
    require(doc.addFoldedSheetMetal(flanges,25.0,1.0,bends,&error)&&doc.parts().size()==1,
            "Could not add folded sheet-metal solid to the scene");
    cad::BodyDiagnostics diagnostics;
    require(doc.diagnoseBody(doc.parts().first().id,&diagnostics,&error),
            "Could not inspect the folded sheet-metal solid");
    require(diagnostics.volume>0.0&&diagnostics.minY<0.0&&diagnostics.maxX>40.0&&
            diagnostics.maxY>30.0&&std::abs((diagnostics.maxZ-diagnostics.minZ)-25.0)<1e-9,
            QStringLiteral("Folded sheet-metal solid has invalid bounds or thickness direction: volume=%1 x=[%2,%3] y=[%4,%5] z=[%6,%7]")
                .arg(diagnostics.volume).arg(diagnostics.minX).arg(diagnostics.maxX)
                .arg(diagnostics.minY).arg(diagnostics.maxY).arg(diagnostics.minZ).arg(diagnostics.maxZ).toStdString().c_str());
    const double firstVolume=diagnostics.volume;
    require(doc.undo()&&doc.parts().isEmpty()&&doc.redo()&&doc.parts().size()==1,
            "Folded sheet-metal creation did not undo and redo as one scene operation");
    cad::BodyDiagnostics reopenedDiagnostics;
    require(doc.diagnoseBody(doc.parts().first().id,&reopenedDiagnostics,&error)&&
            std::abs(reopenedDiagnostics.volume-firstVolume)<firstVolume*1e-8,
            "Folded sheet-metal redo changed the solid geometry");
    const QString scenePath=directory.filePath("folded-sheet.cadscene");
    require(doc.saveAs(scenePath,&error),"Could not save the folded sheet-metal solid");
    cad::SceneDocument reopened;
    require(reopened.load(scenePath,&error)&&reopened.parts().size()==1&&
            reopened.diagnoseBody(reopened.parts().first().id,&reopenedDiagnostics,&error)&&
            std::abs(reopenedDiagnostics.volume-firstVolume)<firstVolume*1e-8,
            "Folded sheet-metal solid did not persist across scene reload");
    QVector<double> savedFlanges;double savedWidth=0.0,savedThickness=0.0;QVector<cad::FoldedSheetBend> savedBends;bool current=false;
    const QString reopenedId=reopened.parts().first().id;
    require(reopened.foldedSheetMetalDefinition(reopenedId,&savedFlanges,&savedWidth,&savedThickness,&savedBends,&current)&&current&&
            savedFlanges==flanges&&savedWidth==25.0&&savedThickness==1.0&&savedBends.size()==1&&
            savedBends.first().angleDegrees==90.0&&savedBends.first().insideRadiusMm==2.0,
            "Folded sheet-metal edit definition did not persist or match the body");
    cad::MultiBendFlatPattern developed;const double expectedAllowance=std::numbers::pi/2.0*(2.0+0.33);
    require(cad::calculateFoldedSheetFlatPattern(savedFlanges,savedWidth,savedThickness,savedBends,0.33,&developed,&error)&&
            std::abs(developed.flatLengthMm-(70.0+expectedAllowance))<1e-9&&
            std::abs(developed.bendLinePositionsMm.first()-(40.0+expectedAllowance/2.0))<1e-9,
            "Folded-body flat pattern did not conserve the straight neutral path and bend allowance");
    QByteArray foldedDxf;require(cad::buildFoldedSheetFlatPatternDxf(savedFlanges,savedWidth,savedThickness,savedBends,0.33,&foldedDxf,&developed,&error)&&
            foldedDxf.contains("$INSUNITS\n70\n4")&&foldedDxf.count("0\nLINE\n8\nBEND\n")==1&&
            foldedDxf.count("0\nLINE\n8\nOUTLINE\n")==4,
            "Folded-body DXF omitted the millimeter blank outline or bend line");
    const QString foldedDxfPath=directory.filePath("folded-body-flat.dxf");
    require(cad::exportFoldedSheetFlatPatternDxf(savedFlanges,savedWidth,savedThickness,savedBends,0.33,foldedDxfPath,&developed,&error)&&
            readBytes(foldedDxfPath)==foldedDxf,
            "Folded-body flat-pattern DXF export did not match its generated geometry");
    const QByteArray validFoldedDxf=foldedDxf;const double validFlatLength=developed.flatLengthMm;
    require(!cad::buildFoldedSheetFlatPatternDxf(savedFlanges,savedWidth,savedThickness,savedBends,1.01,&foldedDxf,&developed,&error)&&
            foldedDxf==validFoldedDxf&&developed.flatLengthMm==validFlatLength,
            "Rejected folded-body K-factor changed previously valid DXF or calculation outputs");
    require(!cad::exportFoldedSheetFlatPatternDxf(savedFlanges,savedWidth,savedThickness,savedBends,1.01,foldedDxfPath,&developed,&error)&&
            readBytes(foldedDxfPath)==validFoldedDxf,
            "Rejected folded-body K-factor overwrote an existing valid DXF file");
    const QVector<cad::FoldedSheetBend> signedBends{{90.0,1.0},{-90.0,1.5}};
    require(cad::calculateFoldedSheetFlatPattern({30.0,30.0,30.0},20.0,1.2,signedBends,0.4,&developed,&error)&&
            developed.bendAllowancesMm.size()==2&&developed.bendLinePositionsMm.size()==2&&
            std::abs(developed.flatLengthMm-(90.0+std::numbers::pi/2.0*(1.0+0.4*1.2)+
                                               std::numbers::pi/2.0*(1.5+0.4*1.2)))<1e-9,
            "Signed multi-bend flat pattern did not use absolute bend development consistently");
    require(cad::buildFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,0.4,&foldedDxf,&developed,&error)&&
            foldedDxf.count("0\nLINE\n8\nBEND\n")==2,
            "Signed multi-bend folded definition did not export its two bend lines");
    const QVector<double> individualK{0.2,0.6};
    require(cad::calculateFoldedSheetFlatPattern({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,&developed,&error)&&
            std::abs(developed.bendAllowancesMm[0]-std::numbers::pi/2.0*(1.0+0.2*1.2))<1e-9&&
            std::abs(developed.bendAllowancesMm[1]-std::numbers::pi/2.0*(1.5+0.6*1.2))<1e-9,
            "Folded sheet did not apply independent K-factors to each bend allowance");
    const double individualLength=developed.flatLengthMm;
    require(cad::buildFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,&foldedDxf,&developed,&error)&&
            foldedDxf.count("0\nLINE\n8\nBEND\n")==2&&std::abs(developed.flatLengthMm-individualLength)<1e-9,
            "Per-bend K-factor DXF did not match its flat-pattern calculation");
    QByteArray relievedFoldedDxf;
    const cad::SheetMetalRelief foldedRelief{4.0,2.5};
    require(cad::buildFoldedSheetFlatPatternDxf(flanges,25.0,1.0,bends,QVector<double>{0.33},
                foldedRelief,&relievedFoldedDxf,&developed,&error)&&
            relievedFoldedDxf.count("0\nLINE\n8\nOUTLINE\n")==12&&
            relievedFoldedDxf.contains("\n20\n2.500000\n")&&
            relievedFoldedDxf.contains("\n21\n22.500000\n"),
            "Single-bend folded DXF omitted end reliefs or failed to trim its bend line");
    require(cad::buildFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,
                foldedRelief,&relievedFoldedDxf,&developed,&error)&&
            relievedFoldedDxf.count("0\nLINE\n8\nOUTLINE\n")==20&&
            relievedFoldedDxf.count("0\nLINE\n8\nBEND\n")==2,
            "Multi-bend folded DXF omitted independent end relief notches");
    const QString relievedFoldedPath=directory.filePath("folded-relieved.dxf");
    require(cad::exportFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,
                foldedRelief,relievedFoldedPath,&developed,&error)&&readBytes(relievedFoldedPath)==relievedFoldedDxf,
            "Relieved folded-sheet DXF file differs from the generated blank");
    const QByteArray preservedRelief=relievedFoldedDxf;
    require(!cad::buildFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,
                cad::SheetMetalRelief{4.0,10.0},&relievedFoldedDxf,&developed,&error)&&
            relievedFoldedDxf==preservedRelief&&std::abs(developed.flatLengthMm-individualLength)<1e-9,
            "Rejected folded relief changed the previous DXF or calculation");
    require(!cad::exportFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,
                cad::SheetMetalRelief{4.0,10.0},relievedFoldedPath,&developed,&error)&&
            readBytes(relievedFoldedPath)==preservedRelief,
            "Rejected folded relief overwrote a previously valid file");
    QByteArray bendTable;
    require(cad::buildFoldedSheetBendTableCsv({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,&bendTable,&error)&&
            bendTable.startsWith("bend_index,direction,angle_degrees,")&&
            bendTable.contains("\n1,UP,90,1,")&&bendTable.contains("\n2,DOWN,-90,1.5,")&&
            bendTable.count('\n')==3,
            "Folded bend table omitted signed directions, radius, or a bend row");
    const QString bendTablePath=directory.filePath("folded-bend-table.csv");
    require(cad::exportFoldedSheetBendTableCsv({30.0,30.0,30.0},20.0,1.2,signedBends,individualK,bendTablePath,&error)&&
            readBytes(bendTablePath)==bendTable,
            "Exported folded bend table differs from its generated data");
    const QByteArray validBendTable=bendTable;
    require(!cad::buildFoldedSheetBendTableCsv({30.0,30.0,30.0},20.0,1.2,signedBends,{0.2},&bendTable,&error)&&
            bendTable==validBendTable,
            "Invalid per-bend K-factor list changed the prior bend table");
    require(!cad::exportFoldedSheetBendTableCsv({30.0,30.0,30.0},20.0,1.2,signedBends,{0.2},bendTablePath,&error)&&
            readBytes(bendTablePath)==validBendTable,
            "Invalid bend table data overwrote an existing CSV file");
    require(!cad::calculateFoldedSheetFlatPattern({30.0,30.0,30.0},20.0,1.2,signedBends,QVector<double>{0.2},&developed,&error)&&
            std::abs(developed.flatLengthMm-individualLength)<1e-9,
            "Incomplete per-bend K-factor list changed the prior calculation");
    require(!cad::buildFoldedSheetFlatPatternDxf({30.0,30.0,30.0},20.0,1.2,signedBends,QVector<double>{0.2,1.01},&foldedDxf,&developed,&error)&&
            std::abs(developed.flatLengthMm-individualLength)<1e-9,
            "Out-of-range per-bend K-factor changed the prior flat-pattern result");
    require(reopened.editFoldedSheetMetal(reopenedId,{42.0,30.0},25.0,1.0,{{90.0,2.0}},&error)&&
            reopened.diagnoseBody(reopenedId,&reopenedDiagnostics,&error)&&reopenedDiagnostics.volume>firstVolume,
            "Editing folded sheet-metal parameters did not rebuild the solid");
    require(reopened.undo()&&reopened.foldedSheetMetalDefinition(reopenedId,&savedFlanges,&savedWidth,&savedThickness,&savedBends,&current)&&
            current&&savedFlanges==flanges&&reopened.redo()&&
            reopened.foldedSheetMetalDefinition(reopenedId,&savedFlanges,&savedWidth,&savedThickness,&savedBends,&current)&&
            current&&savedFlanges.first()==42.0,
            "Folded sheet-metal edit was not undoable and redoable with its definition");
    const QString editedScenePath=directory.filePath("folded-sheet-edited.cadscene");
    require(reopened.saveAs(editedScenePath,&error),"Could not save edited folded sheet-metal body");
    cad::SceneDocument editedReopened;
    require(editedReopened.load(editedScenePath,&error)&&
            editedReopened.foldedSheetMetalDefinition(reopenedId,&savedFlanges,&savedWidth,&savedThickness,&savedBends,&current)&&
            current&&savedFlanges.first()==42.0,
            "Edited folded sheet-metal definition did not survive save and reopen");
    cad::SceneDocument stale;
    require(stale.addFoldedSheetMetal(flanges,25.0,1.0,bends,&error),"Could not create stale-definition fixture");
    const QString staleId=stale.parts().first().id;
    require(stale.scale(staleId,2.0,&error)&&
            stale.foldedSheetMetalDefinition(staleId,&savedFlanges,&savedWidth,&savedThickness,&savedBends,&current)&&!current&&
            !stale.editFoldedSheetMetal(staleId,{42.0,30.0},25.0,1.0,bends,&error),
            "A direct geometry edit did not make the folded definition safely read-only");
    require(doc.addFoldedSheetMetal({30.0,30.0,30.0},20.0,1.2,
                                    {{90.0,1.0},{-90.0,1.5}},&error)&&doc.parts().size()==2,
            "Opposite-direction bends did not create a folded Z-profile");
    const QVector<QPointF> preserved=outline;
    require(!cad::buildFoldedSheetOutline(flanges,25.0,1.0,{{0.0,2.0}},&outline,&error)&&
            outline==preserved,
            "Rejected fold angles changed the prior outline result");
    require(!cad::buildFoldedSheetOutline(flanges,25.0,1.0,{{90.0,0.0}},&outline,&error)&&
            outline==preserved,
            "A zero inside radius created a degenerate folded profile");
    const int oldCount=doc.parts().size();
    require(!doc.addFoldedSheetMetal(flanges,25.0,1.0,{{180.0,2.0}},&error)&&
            doc.parts().size()==oldCount,
            "An unsupported folded profile partially changed the scene");
    require(!doc.addFoldedSheetMetal({40.0},25.0,1.0,bends,&error)&&
            doc.parts().size()==oldCount,
            "A flange/bend count mismatch partially changed the scene");
}

void testParametricLinks() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Dependency graph temp dir unavailable");
    cad::SceneDocument doc;
    require(doc.addParametricRectangularExtrusion(80,40,20,&error),"Link test base failed");
    const QString id=doc.parts().first().id;
    const TopoDS_Shape initial=doc.parts().first().shape;
    const QString token=doc.parts().first().geometryToken;
    require(doc.setParameterLink(id,"height","width",0.5,&error),"Could not create height/width link");
    require(doc.parameterLinks(id).value("height").source=="width"&&
            doc.parts().first().shape.IsSame(initial)&&doc.parts().first().geometryToken==token,
            "Equal-valued link changed geometry or was not recorded");
    require(doc.undo()&&doc.parameterLinks(id).isEmpty()&&doc.redo()&&
            doc.parameterLinks(id).contains("height"),"Metadata-only link was not undoable");
    require(doc.setParameterLink(id,"depth","height",0.5,&error),"Could not chain depth to height");
    require(doc.setNamedParameter(id,"width",100,&error),"Chained dependency rebuild failed");
    closeTo(doc.namedParameters(id).value("height"),50);
    closeTo(doc.namedParameters(id).value("depth"),25);
    closeTo(volume(doc.parts().first().shape),125000);
    require(doc.undo()&&doc.namedParameters(id).value("width")==80&&
            doc.namedParameters(id).value("height")==40&&doc.namedParameters(id).value("depth")==20&&
            doc.redo()&&doc.namedParameters(id).value("depth")==25,
            "Dependency rebuild did not undo and redo as one operation");
    const QString beforeRejected=doc.parts().first().geometryToken;
    require(!doc.setNamedParameter(id,"height",42,&error)&&error.contains("driven")&&
            !doc.setParameterLink(id,"width","depth",3,&error)&&error.contains("Cyclic")&&
            !doc.setParameterLink(id,"width","width",1,&error)&&
            !doc.setParameterLink(id,"hole_x","width",1,&error)&&
            !doc.setParameterLink(id,"hole_diameter","width",1,&error)&&
            !doc.setParameterLink(id,"width","height",0,&error)&&
            doc.parts().first().geometryToken==beforeRejected&&doc.parameterLinks(id).size()==2,
            "Invalid dependency, missing dimension, or cycle changed the scene");
    require(!doc.setNamedParameter(id,"width",1e6+1,&error)&&
            doc.parts().first().geometryToken==beforeRejected&&doc.namedParameters(id).value("width")==100,
            "Out-of-range dependency source edit was not rejected atomically");
    const TopoDS_Shape beforeNoop=doc.parts().first().shape;
    require(doc.setNamedParameter(id,"width",100,&error)&&
            doc.parts().first().shape.IsSame(beforeNoop),
            "Unchanged source value rebuilt an active parameter dependency graph");
    cad::SceneDocument limitDoc;
    require(limitDoc.addParametricRectangularExtrusion(600000,20,10,&error),
            "Could not create upper-bound dependency fixture");
    const QString limitId=limitDoc.parts().first().id;
    const QString limitToken=limitDoc.parts().first().geometryToken;
    require(!limitDoc.setParameterLink(limitId,"height","width",2.0,&error)&&
            limitDoc.parameterLinks(limitId).isEmpty()&&
            limitDoc.parts().first().geometryToken==limitToken,
            "Out-of-range derived dimension changed the document");
    cad::SceneDocument changedOnLink;
    require(changedOnLink.addParametricRectangularExtrusion(50,30,10,&error),
            "Could not create rebuilding dependency fixture");
    const QString changedId=changedOnLink.parts().first().id;
    require(changedOnLink.setParameterLink(changedId,"height","width",0.5,&error)&&
            changedOnLink.namedParameters(changedId).value("height")==25&&
            changedOnLink.undo()&&changedOnLink.parameterLinks(changedId).isEmpty()&&
            changedOnLink.namedParameters(changedId).value("height")==30&&
            changedOnLink.redo()&&changedOnLink.parameterLinks(changedId).contains("height"),
            "Dependency creation with geometry change was not an atomic undoable edit");
    const QString beforeReplacementToken=changedOnLink.parts().first().geometryToken;
    require(changedOnLink.setParameterLink(changedId,"height","width",0.4,&error)&&
            changedOnLink.parameterLinks(changedId).value("height").scale==0.4&&
            changedOnLink.namedParameters(changedId).value("height")==20&&
            changedOnLink.parts().first().geometryToken!=beforeReplacementToken&&
            changedOnLink.undo()&&changedOnLink.parameterLinks(changedId).value("height").scale==0.5&&
            changedOnLink.namedParameters(changedId).value("height")==25&&
            changedOnLink.redo()&&changedOnLink.parameterLinks(changedId).value("height").scale==0.4&&
            changedOnLink.namedParameters(changedId).value("height")==20,
            "Replacing a parameter dependency did not rebuild and undo/redo atomically");
    const QString beforeInvalidReplacement=changedOnLink.parts().first().geometryToken;
    require(!changedOnLink.setParameterLink(changedId,"height","height",1.0,&error)&&
            changedOnLink.parameterLinks(changedId).value("height").scale==0.4&&
            changedOnLink.namedParameters(changedId).value("height")==20&&
            changedOnLink.parts().first().geometryToken==beforeInvalidReplacement,
            "Invalid replacement dependency partially changed the existing link");
    require(changedOnLink.setParameterLink(changedId,"height","width",0.4,&error)&&
            changedOnLink.undo()&&changedOnLink.parameterLinks(changedId).value("height").scale==0.5&&
            changedOnLink.redo()&&changedOnLink.parameterLinks(changedId).value("height").scale==0.4,
            "Reapplying an identical dependency consumed an extra undo checkpoint");
    // Create a hole pattern, then link spacing to the diameter and verify
    // invalid downstream containment rejects the entire source edit.
    cad::SceneDocument pattern;
    require(pattern.addParametricRectangularExtrusion(80,30,20,&error),"Pattern dependency base failed");
    const QString patternId=pattern.parts().first().id;
    require(pattern.addParametricThroughHole(patternId,8,10,15,&error)&&
            pattern.addParametricLinearHolePattern(patternId,3,14,&error),"Pattern dependency fixture failed");
    require(pattern.setParameterLink(patternId,"hole_pattern_spacing","hole_diameter",1.75,&error),
            "Could not link pattern spacing to diameter");
    const QString patternToken=pattern.parts().first().geometryToken;
    require(!pattern.setNamedParameter(patternId,"hole_diameter",28,&error)&&
            pattern.parts().first().geometryToken==patternToken&&
            pattern.namedParameters(patternId).value("hole_diameter")==8,
            "Invalid linked pattern dimensions changed geometry");
    require(pattern.setNamedParameter(patternId,"hole_diameter",10,&error),
            "Valid linked pattern dimensions did not rebuild");
    closeTo(pattern.namedParameters(patternId).value("hole_pattern_spacing"),17.5);

    // Affine links can encode practical clearance and stock allowance rules.
    cad::SceneDocument affine;
    require(affine.addParametricRectangularExtrusion(80,40,20,&error),"Affine-link fixture base failed");
    const QString affineId=affine.parts().first().id;
    require(affine.setParameterLink(affineId,"height","width",0.5,5.0,&error)&&
            affine.parameterLinks(affineId).value("height").offset==5.0&&
            affine.namedParameters(affineId).value("height")==45.0,
            "Offset parameter link did not apply target = source × scale + offset");
    require(affine.setParameterLink(affineId,"depth","height",0.5,2.0,&error)&&
            affine.namedParameters(affineId).value("depth")==24.5,
            "Chained scale-and-offset dependencies did not resolve source-first");
    require(affine.setNamedParameter(affineId,"width",100,&error)&&
            affine.namedParameters(affineId).value("height")==55.0&&
            affine.namedParameters(affineId).value("depth")==29.5,
            "Editing an upstream source did not rebuild chained offset dependencies");
    closeTo(volume(affine.parts().first().shape),100.0*55.0*29.5);
    require(affine.undo()&&affine.namedParameters(affineId).value("height")==45.0&&
            affine.namedParameters(affineId).value("depth")==24.5&&
            affine.redo()&&affine.namedParameters(affineId).value("depth")==29.5,
            "Chained offset-driven geometry did not undo and redo atomically");
    const QString affinePath=dir.filePath("affine-parameter-links.cadscene");
    require(affine.saveAs(affinePath,&error),"Could not save affine parameter links");
    require(QJsonDocument::fromJson(readBytes(affinePath)).object().value("schema").toString()=="cadworkbench.scene.v22",
            "Offset parameter links did not trigger the new scene schema");
    cad::SceneDocument affineLoaded;
    require(affineLoaded.load(affinePath,&error)&&
            affineLoaded.parameterLinks(affineId).value("height").offset==5.0&&
            affineLoaded.parameterLinks(affineId).value("depth").offset==2.0&&
            affineLoaded.namedParameters(affineId)==affine.namedParameters(affineId),
            "Affine parameter links did not survive save/reopen");
    const QString affineToken=affineLoaded.parts().first().geometryToken;
    require(!affineLoaded.setParameterLink(affineId,"height","width",0.5,1e6+1.0,&error)&&
            !affineLoaded.setParameterLink(affineId,"height","width",0.5,std::numeric_limits<double>::infinity(),&error)&&
            affineLoaded.parts().first().geometryToken==affineToken&&
            affineLoaded.parameterLinks(affineId).value("height").offset==5.0,
            "Invalid or out-of-range affine-link offset changed the existing relationship");
    QJsonObject affineRoot=QJsonDocument::fromJson(readBytes(affinePath)).object();
    QJsonArray affineParts=affineRoot.value("parts").toArray();QJsonObject affinePart=affineParts[0].toObject();
    QJsonObject affineDef=affinePart.value("parametric_definition").toObject();
    QJsonObject affineLinks=affineDef.value("parameter_links").toObject();
    QJsonObject affineHeight=affineLinks.value("height").toObject();affineHeight.insert("offset",8.0);
    affineLinks.insert("height",affineHeight);affineDef.insert("parameter_links",affineLinks);
    affinePart.insert("parametric_definition",affineDef);affineParts[0]=affinePart;affineRoot.insert("parts",affineParts);
    const QString forgedAffinePath=dir.filePath("tampered-affine-link.cadscene");
    writeBytes(forgedAffinePath,QJsonDocument(affineRoot).toJson());
    require(!affineLoaded.load(forgedAffinePath,&error)&&error.contains("disagree")&&
            affineLoaded.parameterLinks(affineId).value("height").offset==5.0,
            "Tampered saved affine offset was accepted or partially replaced the open scene");
    affineRoot.insert("schema","cadworkbench.scene.v19");
    const QString unsupportedV19Offset=dir.filePath("v19-affine-link.cadscene");
    writeBytes(unsupportedV19Offset,QJsonDocument(affineRoot).toJson());
    require(!affineLoaded.load(unsupportedV19Offset,&error)&&affineLoaded.parameterLinks(affineId).value("height").offset==5.0,
            "Schema v19 accepted an offset field introduced in schema v20");

    const QString path=dir.filePath("parameter-links.cadscene");
    require(doc.saveAs(path,&error),"Could not save linked dimensions");
    QJsonObject zeroOffsetV19=QJsonDocument::fromJson(readBytes(path)).object();
    zeroOffsetV19.insert("schema","cadworkbench.scene.v19");
    const QString zeroOffsetV19Path=dir.filePath("zero-offset-links-v19.cadscene");
    writeBytes(zeroOffsetV19Path,QJsonDocument(zeroOffsetV19).toJson());
    cad::SceneDocument zeroOffsetV19Loaded;
    require(zeroOffsetV19Loaded.load(zeroOffsetV19Path,&error)&&zeroOffsetV19Loaded.parameterLinks(id).size()==2&&
            zeroOffsetV19Loaded.parameterLinks(id).value("height").offset==0.0,
            "Schema-v19 zero-offset parameter links failed to migrate into the new loader");
    cad::SceneDocument loaded;
    require(loaded.load(path,&error)&&loaded.parameterLinks(id).size()==2&&
            loaded.parameterLinks(id).value("depth").source=="height"&&
            loaded.namedParameters(id)==doc.namedParameters(id),"Linked dimensions did not round-trip");
    // Tampering with a saved ratio while preserving values and B-rep must fail.
    QJsonObject root=QJsonDocument::fromJson(readBytes(path)).object();
    QJsonArray parts=root.value("parts").toArray();QJsonObject part=parts[0].toObject();
    QJsonObject definition=part.value("parametric_definition").toObject();
    QJsonObject links=definition.value("parameter_links").toObject();
    QJsonObject height=links.value("height").toObject();height.insert("scale",0.75);
    links.insert("height",height);definition.insert("parameter_links",links);
    part.insert("parametric_definition",definition);parts[0]=part;root.insert("parts",parts);
    const QString forged=dir.filePath("tampered-link.cadscene");
    writeBytes(forged,QJsonDocument(root).toJson());
    require(!loaded.load(forged,&error)&&error.contains("disagree")&&
            loaded.parameterLinks(id).value("height").scale==0.5,
            "Forged saved dependency was accepted or partially replaced the document");
    // Cycle must be rejected before any saved scene replacement.
    QJsonObject cyclicRoot=QJsonDocument::fromJson(readBytes(path)).object();
    QJsonArray cyclicParts=cyclicRoot.value("parts").toArray();
    QJsonObject cyclicPart=cyclicParts[0].toObject();
    QJsonObject cyclicDef=cyclicPart.value("parametric_definition").toObject();
    QJsonObject cyclicLinks=cyclicDef.value("parameter_links").toObject();
    cyclicLinks.insert("width",QJsonObject{{"source","depth"},{"scale",4.0}});
    cyclicDef.insert("parameter_links",cyclicLinks);
    cyclicPart.insert("parametric_definition",cyclicDef);cyclicParts[0]=cyclicPart;
    cyclicRoot.insert("parts",cyclicParts);
    const QString cyclicPath=dir.filePath("cyclic-link.cadscene");
    writeBytes(cyclicPath,QJsonDocument(cyclicRoot).toJson());
    require(!loaded.load(cyclicPath,&error)&&error.contains("Cyclic")&&
            loaded.parameterLinks(id).size()==2,"Cyclic saved dependency changed the document");
    const QString beforeRemove=loaded.parts().first().geometryToken;
    require(loaded.removeParameterLink(id,"depth",&error)&&
            loaded.parts().first().geometryToken==beforeRemove&&
            loaded.namedParameters(id).value("depth")==25,
            "Removing a dependency altered solid identity or target value");
    require(loaded.undo()&&loaded.parameterLinks(id).contains("depth")&&
            loaded.redo()&&!loaded.parameterLinks(id).contains("depth"),
            "Dependency removal was not undoable");
    require(loaded.setNamedParameter(id,"width",120,&error),"Post-detach source edit failed");
    closeTo(loaded.namedParameters(id).value("height"),60);
    closeTo(loaded.namedParameters(id).value("depth"),25);
    require(loaded.translate(id,1,0,0,&error)&&
            !loaded.setParameterLink(id,"depth","height",0.5,&error)&&
            !loaded.removeParameterLink(id,"height",&error),
            "Stale parametric history permitted link edits");
}

void testParametricBlindHolePattern() {
    QString error;cad::SceneDocument doc;
    require(doc.addParametricRectangularExtrusion(70,30,20,&error),"Could not create blind-pattern base");
    const QString id=doc.parts().first().id;
    require(doc.addParametricBlindHole(id,8,10,15,8,&error),"Could not create blind-pattern seed hole");
    const QString beforeInvalid=doc.find(id)->geometryToken;
    require(!doc.addParametricLinearHolePattern(id,3,7,&error)&&doc.find(id)->geometryToken==beforeInvalid,
            "Overlapping blind-hole pattern changed the solid");
    require(doc.addParametricLinearHolePattern(id,3,14,"X",&error),
            ("Could not create linear blind-hole pattern: "+error.toStdString()).c_str());
    require(doc.featureHistory(id).last()=="Pattern: Linear blind-hole array along X"&&
            doc.namedParameters(id).value("hole_pattern_count")==3&&
            std::abs(volume(doc.find(id)->shape)-(42000-3*std::numbers::pi*16*8))<1e-5,
            "Blind-hole pattern has wrong history, count, or volume");
    require(doc.setNamedParameter(id,"hole_depth",10,&error)&&
            std::abs(volume(doc.find(id)->shape)-(42000-3*std::numbers::pi*16*10))<1e-5,
            "Changing the named blind-hole depth did not rebuild the full pattern");
    require(doc.setParametricFeatureSuppressed(id,"linear_hole_pattern",true,&error)&&
            std::abs(volume(doc.find(id)->shape)-(42000-std::numbers::pi*16*10))<1e-5,
            "Suppressing the blind-hole pattern did not retain the seed cut");
    require(doc.setParametricFeatureSuppressed(id,"blind_hole",true,&error)&&
            std::abs(volume(doc.find(id)->shape)-42000)<1e-5,
            "Suppressing the blind-hole cut did not remove all pattern cuts");
    require(doc.setParametricFeatureSuppressed(id,"blind_hole",false,&error)&&
            doc.setParametricFeatureSuppressed(id,"linear_hole_pattern",false,&error),
            "Could not restore blind-hole pattern and seed");
    QTemporaryDir dir;require(dir.isValid(),"Could not create blind-pattern test directory");
    const QString path=dir.filePath("blind-pattern.cadscene");
    require(doc.saveAs(path,&error),"Could not save blind-hole pattern");
    cad::SceneDocument loaded;
    require(loaded.load(path,&error),("Could not reopen blind-hole pattern: "+error.toStdString()).c_str());
    require(loaded.featureHistory(id)==doc.featureHistory(id)&&loaded.namedParameters(id)==doc.namedParameters(id)&&
            std::abs(volume(loaded.find(id)->shape)-(42000-3*std::numbers::pi*16*10))<1e-5,
            "Blind-hole pattern did not survive scene persistence");
}

void testParametricBlindHole() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Blind-hole test folder unavailable");
    cad::SceneDocument doc;
    require(doc.addParametricRectangularExtrusion(40,30,20,&error),"Blind-hole parametric base failed");
    const QString id=doc.parts().first().id;
    require(!doc.addParametricBlindHole(id,8,20,15,20,&error)&&doc.featureHistory(id).size()==2,
            "Blind hole accepted a through-all depth or changed history");
    require(!doc.addParametricBlindHole(id,8,2,15,5,&error)&&doc.featureHistory(id).size()==2,
            "Blind hole outside the constrained profile changed feature history");
    require(doc.addParametricBlindHole(id,8,20,15,6,&error),
            ("Could not create editable parametric blind hole: "+error.toStdString()).c_str());
    require(doc.featureHistory(id).size()==4&&doc.sketchConstraints(id).size()==9&&
            doc.featureHistory(id).last()=="Cut: Parametric blind hole",
            "Blind-hole sketch dimensions and cut operation were not recorded");
    closeTo(doc.namedParameters(id).value("hole_depth"),6);
    closeTo(volume(doc.parts().first().shape),24000-std::numbers::pi*16*6,1e-5);
    require(doc.setNamedParameter(id,"hole_depth",10,&error),"Blind-hole depth parameter could not rebuild the solid");
    closeTo(volume(doc.parts().first().shape),24000-std::numbers::pi*16*10,1e-5);
    require(doc.setNamedParameters(id,{{"hole_diameter",10},{"hole_depth",8}},&error),
            "Coordinated blind-hole dimension edit failed");
    closeTo(volume(doc.parts().first().shape),24000-std::numbers::pi*25*8,1e-5);
    require(!doc.setNamedParameter(id,"hole_depth",20,&error)&&doc.namedParameters(id).value("hole_depth")==8,
            "Blind-hole depth reaching the far face was accepted");
    require(doc.setParameterLink(id,"hole_depth","hole_diameter",1.0,2.0,&error)&&
            doc.namedParameters(id).value("hole_depth")==12,
            "Blind-hole depth could not use a linked diameter-plus-stock allowance");
    require(doc.setNamedParameter(id,"hole_diameter",11,&error)&&doc.namedParameters(id).value("hole_depth")==13,
            "Blind-hole depth did not follow its linked diameter during rebuild");
    require(doc.undo()&&doc.namedParameters(id).value("hole_diameter")==10&&doc.namedParameters(id).value("hole_depth")==12&&
            doc.redo()&&doc.namedParameters(id).value("hole_depth")==13,
            "Linked blind-hole edit was not one undoable rebuild");
    require(doc.setParametricFeatureSuppressed(id,"blind_hole",true,&error),
            "Blind-hole feature could not be suppressed");
    closeTo(volume(doc.parts().first().shape),24000,1e-5);
    require(doc.setParametricFeatureSuppressed(id,"blind_hole",false,&error),
            "Blind-hole feature could not be restored");
    closeTo(volume(doc.parts().first().shape),24000-std::numbers::pi*30.25*13,1e-5);
    const QString path=dir.filePath("parametric-blind-hole.cadscene");
    require(doc.saveAs(path,&error),"Could not save blind-hole feature history");
    const QJsonObject root=QJsonDocument::fromJson(readBytes(path)).object();
    require(root.value("schema").toString()=="cadworkbench.scene.v22",
            "Blind-hole history did not use schema v22");
    cad::SceneDocument loaded;
    require(loaded.load(path,&error)&&loaded.featureHistory(id)==doc.featureHistory(id)&&
            loaded.sketchConstraints(id)==doc.sketchConstraints(id)&&loaded.namedParameters(id)==doc.namedParameters(id)&&
            loaded.parameterLinks(id).value("hole_depth").offset==2,
            ("Blind-hole sketch, parameters, or affine link failed save/reopen: "+error.toStdString()).c_str());
    closeTo(volume(loaded.parts().first().shape),24000-std::numbers::pi*30.25*13,1e-5);
    QJsonObject forged=root;QJsonArray parts=forged.value("parts").toArray();QJsonObject part=parts[0].toObject();
    QJsonObject definition=part.value("parametric_definition").toObject();QJsonObject parameters=definition.value("parameters").toObject();
    parameters.insert("hole_y",20.0);definition.insert("parameters",parameters);part.insert("parametric_definition",definition);
    parts[0]=part;forged.insert("parts",parts);
    const QString badPath=dir.filePath("tampered-blind-hole.cadscene");
    writeBytes(badPath,QJsonDocument(forged).toJson());
    require(!loaded.load(badPath,&error)&&loaded.namedParameters(id).value("hole_y")==15,
            "Tampered blind-hole center was accepted or replaced the open document");
    forged.insert("schema","cadworkbench.scene.v20");
    const QString oldSchemaPath=dir.filePath("blind-hole-v20.cadscene");
    writeBytes(oldSchemaPath,QJsonDocument(forged).toJson());
    require(!loaded.load(oldSchemaPath,&error)&&loaded.namedParameters(id).value("hole_y")==15,
            "Schema v20 accepted a blind-hole feature introduced in v21");
}

void testParametricFeatureSuppression() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Feature-suppression test folder failed");
    cad::SceneDocument doc;
    require(doc.addParametricRectangularExtrusion(80,40,20,&error),"Could not create feature-suppression base");
    const QString id=doc.parts().first().id;
    require(doc.addParametricThroughHole(id,10,20,20,&error)&&
            doc.addParametricLinearHolePattern(id,3,20,&error),"Could not create suppressible hole operations");
    const double patternedVolume=volume(doc.parts().first().shape);
    require(doc.setParametricFeatureSuppressed(id,"linear_hole_pattern",true,&error)&&
            doc.suppressedParametricFeatures(id).contains("linear_hole_pattern"),
            "Could not suppress a linear pattern operation");
    closeTo(volume(doc.parts().first().shape),64000.0-std::numbers::pi*25.0*20.0);
    require(doc.undo()&&doc.suppressedParametricFeatures(id).isEmpty(),"Pattern suppression was not undoable");
    closeTo(volume(doc.parts().first().shape),patternedVolume);
    require(doc.redo()&&doc.suppressedParametricFeatures(id).contains("linear_hole_pattern"),
            "Pattern suppression could not be redone");
    require(doc.setParametricFeatureSuppressed(id,"through_hole",true,&error),
            "Could not suppress a hole and its dependent pattern result");
    closeTo(volume(doc.parts().first().shape),64000.0);
    const QString path=dir.filePath("suppressed-history.cadscene");
    require(doc.saveAs(path,&error),"Could not save suppressed feature history");
    cad::SceneDocument loaded;require(loaded.load(path,&error),"Could not reopen suppressed feature history");
    require(loaded.suppressedParametricFeatures(id)==QStringList({"linear_hole_pattern","through_hole"}),
            "Suppressed feature states did not persist in stable operation order");
    closeTo(volume(loaded.parts().first().shape),64000.0);
    require(loaded.setParametricFeatureSuppressed(id,"through_hole",false,&error),
            "Could not restore a suppressed hole feature");
    closeTo(volume(loaded.parts().first().shape),64000.0-std::numbers::pi*25.0*20.0);
    require(!loaded.setParametricFeatureSuppressed(id,"extrude",true,&error)&&
            loaded.parts().first().geometryToken==loaded.parts().first().parametricGeometryToken,
            "An unsupported base-feature suppression changed the model");
    const double seedOnlyVolume=volume(loaded.parts().first().shape);
    require(loaded.setParametricFeaturesSuppressed(id,{"through_hole","linear_hole_pattern"},&error)&&
            loaded.suppressedParametricFeatures(id)==QStringList({"linear_hole_pattern","through_hole"}),
            "Batch feature suppression failed to normalize and apply two operations");
    closeTo(volume(loaded.parts().first().shape),64000.0);
    require(loaded.undo()&&loaded.suppressedParametricFeatures(id)==QStringList({"linear_hole_pattern"}),
            "Batch suppression did not undo as one model change");
    closeTo(volume(loaded.parts().first().shape),seedOnlyVolume);
    require(loaded.redo()&&loaded.suppressedParametricFeatures(id).size()==2,
            "Batch suppression could not be redone as one model change");
    const QString batchToken=loaded.find(id)->geometryToken;
    require(loaded.setParametricFeaturesSuppressed(id,{"through_hole","linear_hole_pattern"},&error)&&
            loaded.find(id)->geometryToken==batchToken,
            "No-op batch suppression rebuilt an unchanged model");
    require(!loaded.setParametricFeaturesSuppressed(id,{"through_hole","through_hole"},&error)&&
            !loaded.setParametricFeaturesSuppressed(id,{"through_hole","missing_feature"},&error)&&
            loaded.find(id)->geometryToken==batchToken,
            "Invalid batch suppression changed the solid or accepted duplicate and unavailable operations");
    require(loaded.setParametricFeaturesSuppressed(id,{},&error)&&
            loaded.suppressedParametricFeatures(id).isEmpty(),
            "Batch feature restore did not rebuild the full pattern");
    closeTo(volume(loaded.parts().first().shape),patternedVolume);

    cad::SceneDocument chamfered;
    require(chamfered.addParametricRectangularExtrusion(40,30,20,&error),
            "Could not create a parametric chamfer fixture");
    const QString chamferId=chamfered.parts().first().id;
    const double unchamferedVolume=volume(chamfered.parts().first().shape);
    require(!chamfered.addParametricAllEdgeChamfer(chamferId,0,&error)&&chamfered.parts().size()==1,
            "Invalid parametric chamfer distance changed the scene");
    error.clear();
    require(chamfered.addParametricAllEdgeChamfer(chamferId,0.5,&error)&&
            chamfered.featureHistory(chamferId).contains("Chamfer: All edges")&&
            chamfered.namedParameters(chamferId).value("chamfer_distance")==0.5,
            ("Could not add an editable all-edge chamfer: "+error.toStdString()).c_str());
    const double chamferedVolume=volume(chamfered.parts().first().shape);
    require(chamferedVolume<unchamferedVolume,"The parametric chamfer did not remove material");
    require(!chamfered.addParametricAllEdgeChamfer(chamferId,0.25,&error)&&
            volume(chamfered.parts().first().shape)==chamferedVolume,
            "A duplicate parametric chamfer changed the model");
    require(chamfered.setNamedParameter(chamferId,"depth",24,&error),
            "The parametric chamfer failed to rebuild after its base extrusion changed");
    const double deeperChamferedVolume=volume(chamfered.parts().first().shape);
    require(deeperChamferedVolume<40.0*30.0*24.0,
            "Editing the base parameter dropped the downstream chamfer");
    require(chamfered.setParametricFeatureSuppressed(chamferId,"edge_chamfer",true,&error),
            "Could not suppress the parametric chamfer");
    closeTo(volume(chamfered.parts().first().shape),40.0*30.0*24.0);
    require(chamfered.undo(),"Parametric chamfer suppression could not be undone");
    closeTo(volume(chamfered.parts().first().shape),deeperChamferedVolume);
    require(chamfered.redo(),"Parametric chamfer suppression could not be redone");
    closeTo(volume(chamfered.parts().first().shape),40.0*30.0*24.0);
    require(chamfered.setParametricFeatureSuppressed(chamferId,"edge_chamfer",false,&error),
            "Could not restore the parametric chamfer");
    const QString chamferPath=dir.filePath("parametric-chamfer.cadscene");
    require(chamfered.saveAs(chamferPath,&error),"Could not save the editable parametric chamfer");
    error.clear();
    cad::SceneDocument reopenedChamfer;
    require(reopenedChamfer.load(chamferPath,&error),
            ("Could not reopen the editable parametric chamfer: "+error.toStdString()).c_str());
    require(reopenedChamfer.featureHistory(chamferId).contains("Chamfer: All edges"),
            ("Saved parametric feature history is: "+reopenedChamfer.featureHistory(chamferId).join(" | ").toStdString()).c_str());
    closeTo(reopenedChamfer.namedParameters(chamferId).value("chamfer_distance"),0.5);
    closeTo(volume(reopenedChamfer.parts().first().shape),deeperChamferedVolume);

    cad::SceneDocument filleted;
    require(filleted.addParametricRectangularExtrusion(40,30,20,&error),
            "Could not create a parametric fillet fixture");
    const QString filletId=filleted.parts().first().id;
    const double unfilletedVolume=volume(filleted.parts().first().shape);
    require(!filleted.addParametricAllEdgeFillet(filletId,0,&error)&&filleted.parts().size()==1,
            "Invalid parametric fillet radius changed the scene");
    const QString unmodifiedFilletToken=filleted.parts().first().geometryToken;
    error.clear();
    require(!filleted.addParametricAllEdgeFillet(filletId,10.0,&error)&&
            filleted.parts().first().geometryToken==unmodifiedFilletToken&&
            volume(filleted.parts().first().shape)==unfilletedVolume,
            "An out-of-range fillet radius changed the existing body");
    error.clear();
    require(filleted.addParametricAllEdgeFillet(filletId,0.5,&error)&&
            filleted.featureHistory(filletId).contains("Fillet: All edges")&&
            filleted.namedParameters(filletId).value("fillet_radius")==0.5,
            ("Could not add an editable all-edge fillet: "+error.toStdString()).c_str());
    const double initialFilletedVolume=volume(filleted.parts().first().shape);
    require(initialFilletedVolume<unfilletedVolume,
            "The parametric all-edge fillet did not change the solid volume");
    require(filleted.setNamedParameter(filletId,"fillet_radius",0.75,&error),
            "Could not edit the named parametric fillet radius");
    const double filletedVolume=volume(filleted.parts().first().shape);
    require(filletedVolume<initialFilletedVolume,
            "Increasing the named fillet radius did not rebuild the solid");
    error.clear();
    require(!filleted.addParametricAllEdgeChamfer(filletId,0.25,&error)&&
            volume(filleted.parts().first().shape)==filletedVolume,
            "A second incompatible all-edge blend changed the model");
    require(!filleted.addParametricAllEdgeFillet(filletId,0.25,&error)&&
            volume(filleted.parts().first().shape)==filletedVolume,
            "A duplicate parametric all-edge fillet changed the model");
    require(filleted.setNamedParameter(filletId,"depth",24,&error),
            "The parametric fillet failed to rebuild after its base extrusion changed");
    const double deeperFilletedVolume=volume(filleted.parts().first().shape);
    require(deeperFilletedVolume<40.0*30.0*24.0,
            "Editing the base parameter dropped the downstream fillet");
    require(filleted.setParametricFeatureSuppressed(filletId,"edge_fillet",true,&error),
            "Could not suppress the parametric all-edge fillet");
    closeTo(volume(filleted.parts().first().shape),40.0*30.0*24.0);
    require(filleted.undo(),"Parametric fillet suppression was not undoable");
    closeTo(volume(filleted.parts().first().shape),deeperFilletedVolume);
    require(filleted.redo(),"Parametric fillet suppression could not be redone");
    closeTo(volume(filleted.parts().first().shape),40.0*30.0*24.0);
    require(filleted.setParametricFeatureSuppressed(filletId,"edge_fillet",false,&error),
            "Could not restore the parametric all-edge fillet");
    const QString filletPath=dir.filePath("parametric-fillet.cadscene");
    require(filleted.saveAs(filletPath,&error),"Could not save the editable parametric fillet");
    error.clear();
    cad::SceneDocument reopenedFillet;
    require(reopenedFillet.load(filletPath,&error),
            ("Could not reopen the editable parametric fillet: "+error.toStdString()).c_str());
    require(reopenedFillet.featureHistory(filletId).contains("Fillet: All edges"),
            "Saved parametric feature history lost the all-edge fillet");
    closeTo(reopenedFillet.namedParameters(filletId).value("fillet_radius"),0.75);
    closeTo(volume(reopenedFillet.parts().first().shape),deeperFilletedVolume);

    cad::SceneDocument blendedHistory;
    require(blendedHistory.addParametricRectangularExtrusion(40,30,20,&error),
            "Could not create a combined parametric blend fixture");
    const QString blendedId=blendedHistory.parts().first().id;
    require(blendedHistory.addParametricAllEdgeChamfer(blendedId,0.5,&error),
            "Could not add the first blend to a combined parametric history");
    error.clear();
    require(blendedHistory.addParametricAllEdgeFillet(blendedId,0.25,&error),
            ("Could not add a fillet after the parametric chamfer: "+error.toStdString()).c_str());
    const QStringList combinedHistory=blendedHistory.featureHistory(blendedId);
    require(combinedHistory.size()>=2&&combinedHistory[combinedHistory.size()-2]=="Chamfer: All edges"&&
            combinedHistory.last()=="Fillet: All edges",
            "The combined parametric blend history recorded the wrong operation order");
    const double chamferAndFilletVolume=volume(blendedHistory.parts().first().shape);
    require(blendedHistory.setParametricFeatureSuppressed(blendedId,"edge_fillet",true,&error),
            "Could not suppress the second operation in a combined blend history");
    require(volume(blendedHistory.parts().first().shape)>chamferAndFilletVolume,
            "Suppressing the downstream fillet did not restore the chamfer-only solid");
    require(blendedHistory.setParametricFeatureSuppressed(blendedId,"edge_fillet",false,&error),
            "Could not restore the downstream fillet in a combined blend history");
    const QString combinedPath=dir.filePath("parametric-chamfer-fillet.cadscene");
    require(blendedHistory.saveAs(combinedPath,&error),"Could not save both parametric blend operations");
    error.clear();cad::SceneDocument reopenedBlended;
    const bool loadedCombined=reopenedBlended.load(combinedPath,&error);
    require(loadedCombined,
            ("Could not reopen both parametric blend operations: "+error.toStdString()).c_str());
    const QStringList reopenedCombinedHistory=reopenedBlended.featureHistory(blendedId);
    require(reopenedCombinedHistory.size()>=2&&reopenedCombinedHistory[reopenedCombinedHistory.size()-2]=="Chamfer: All edges"&&
            reopenedCombinedHistory.last()=="Fillet: All edges",
            "Save/reopen changed the combined parametric blend history order");
    closeTo(volume(reopenedBlended.parts().first().shape),chamferAndFilletVolume);
}

void testAssemblyJointCoordinateGuards() {
    QString error;
    // Parent translation must not move a nested joint frame beyond the limit.
    cad::SceneDocument translated;
    require(translated.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create translation-limit fixture");
    const QString translationRoot=translated.parts().first().id;
    require(translated.makeComponent(translationRoot,"Translation root",&error)&&
            translated.insertComponentOccurrence(translationRoot,20,0,0,&error),
            "Could not create translation-limit child");
    const QString translationChild=translated.parts().last().id;
    require(translated.addRevoluteJoint(translationRoot,translationChild,999999,0,0,0,0,1,-90,90,&error),
            "Could not create translation-limit revolute joint");
    require(!translated.translate(translationRoot,5,0,0,&error)&&
            translated.find(translationRoot)->placementX==0&&
            translated.find(translationChild)->placementX==20&&
            translated.find(translationChild)->jointOriginX==999999,
            "Out-of-range nested joint translation mutated the scene");

    // Rotation may cross a frame limit even when every occurrence placement stays in range.
    cad::SceneDocument rotated;
    require(rotated.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create rotation-limit fixture");
    const QString rotationRoot=rotated.parts().first().id;
    require(rotated.makeComponent(rotationRoot,"Rotation root",&error)&&
            rotated.insertComponentOccurrence(rotationRoot,20,0,0,&error),
            "Could not create rotation-limit child");
    const QString rotationChild=rotated.parts().last().id;
    require(rotated.addRevoluteJoint(rotationRoot,rotationChild,0,900000,900000,0,0,1,-90,90,&error),
            "Could not create rotation-limit joint");
    require(!rotated.rotate(rotationRoot,cad::Axis::X,45,&error)&&
            rotated.find(rotationRoot)->placementX==0&&
            rotated.find(rotationChild)->placementX==20&&
            rotated.find(rotationChild)->jointOriginY==900000&&
            rotated.find(rotationChild)->jointOriginZ==900000,
            "Rotation crossing the nested joint frame limit mutated the scene");

    // Prismatic travel must validate descendant joint frames, not just placements.
    cad::SceneDocument slider;
    require(slider.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create slider-limit fixture");
    const QString sliderRoot=slider.parts().first().id;
    require(slider.makeComponent(sliderRoot,"Slider root",&error)&&
            slider.insertComponentOccurrence(sliderRoot,20,0,0,&error),
            "Could not create slider-limit child");
    const QString sliderChild=slider.parts().last().id;
    require(slider.insertComponentOccurrence(sliderRoot,40,0,0,&error),
            "Could not create slider-limit leaf");
    const QString sliderLeaf=slider.parts().last().id;
    require(slider.addPrismaticJoint(sliderRoot,sliderChild,1,0,0,-20,20,&error)&&
            slider.addRevoluteJoint(sliderChild,sliderLeaf,999999,0,0,0,0,1,-90,90,&error),
            "Could not create slider-limit nested joints");
    require(!slider.drivePrismaticJoint(sliderChild,10,&error)&&
            slider.find(sliderChild)->jointTranslationMm==0&&
            slider.find(sliderChild)->placementX==20&&
            slider.find(sliderLeaf)->placementX==40&&
            slider.find(sliderLeaf)->jointOriginX==999999,
            "Prismatic drive crossing a descendant joint frame limit mutated the scene");

    // Revolute drives must likewise preflight every descendant frame.
    cad::SceneDocument hinge;
    require(hinge.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create hinge-limit fixture");
    const QString hingeRoot=hinge.parts().first().id;
    require(hinge.makeComponent(hingeRoot,"Hinge root",&error)&&
            hinge.insertComponentOccurrence(hingeRoot,20,0,0,&error),
            "Could not create hinge-limit child");
    const QString hingeChild=hinge.parts().last().id;
    require(hinge.insertComponentOccurrence(hingeRoot,40,0,0,&error),
            "Could not create hinge-limit leaf");
    const QString hingeLeaf=hinge.parts().last().id;
    require(hinge.addRevoluteJoint(hingeRoot,hingeChild,0,0,0,0,0,1,-90,90,&error)&&
            hinge.addRevoluteJoint(hingeChild,hingeLeaf,750000,750000,0,0,0,1,-90,90,&error),
            "Could not create hinge-limit nested joints");
    require(!hinge.driveRevoluteJoint(hingeChild,-45,&error)&&
            hinge.find(hingeChild)->jointAngleDegrees==0&&
            hinge.find(hingeChild)->placementX==20&&
            hinge.find(hingeLeaf)->placementX==40&&
            hinge.find(hingeLeaf)->jointOriginX==750000,
            "Revolute drive crossing a descendant joint frame limit mutated the scene");

    // A duplicate may have a small delta but an out-of-range resulting placement.
    cad::SceneDocument duplicate;
    require(duplicate.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create occurrence duplication-limit fixture");
    const QString duplicateRoot=duplicate.parts().first().id;
    require(duplicate.makeComponent(duplicateRoot,"Duplication root",&error)&&
            duplicate.insertComponentOccurrence(duplicateRoot,999999,0,0,&error),
            "Could not create occurrence near the placement limit");
    const QString nearBoundary=duplicate.parts().last().id;
    require(!duplicate.duplicate(nearBoundary,5,0,0,&error)&&duplicate.parts().size()==2&&
            duplicate.find(nearBoundary)->placementX==999999,
            "Occurrence duplicate accepted an out-of-range resulting placement");
}

void testAssemblyJointPersistenceGuards() {
    QString error; QTemporaryDir dir;
    require(dir.isValid(),"Could not create joint-persistence regression directory");
    cad::SceneDocument model;
    require(model.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create joint-persistence base body");
    const QString parent=model.parts().first().id;
    require(model.makeComponent(parent,"Joint persistence bracket",&error)&&
            model.insertComponentOccurrence(parent,25,0,0,&error),
            "Could not create component occurrences for joint-persistence regression");
    const QString child=model.parts().last().id;

    require(!model.addRevoluteJoint(parent,child,0,0,0,0,0,1,10,90,&error)&&
            model.find(child)->assemblyJointType.isEmpty(),
            "Positive-only revolute limits created a joint whose zero pose cannot reload");
    require(!model.addRevoluteJoint(parent,child,0,0,0,0,0,1,-90,-10,&error)&&
            model.find(child)->assemblyJointType.isEmpty(),
            "Negative-only revolute limits created a joint whose zero pose cannot reload");
    require(model.undo()&&model.parts().size()==1&&model.redo()&&model.parts().size()==2,
            "Rejected revolute joint creation polluted undo/redo history");

    // Both boundary-inclusive ranges are valid: a new joint starts at zero degrees.
    require(model.addRevoluteJoint(parent,child,0,0,0,0,0,1,0,90,&error),
            "Valid revolute joint with zero as its lower limit was rejected");
    const QString path=dir.filePath("zero-inclusive-revolute.cadscene");
    require(model.saveAs(path,&error),"Could not save a zero-inclusive revolute joint");
    cad::SceneDocument reopened;
    require(reopened.load(path,&error),"Zero-inclusive revolute joint did not survive save/load");
    require(reopened.find(child)&&reopened.find(child)->assemblyJointType=="Revolute"&&
            reopened.find(child)->jointAngleDegrees==0&&reopened.find(child)->jointMinimumDegrees==0,
            "Reopened revolute joint lost its initial pose or limits");
    require(reopened.driveRevoluteJoint(child,90,&error)&&reopened.driveRevoluteJoint(child,0,&error),
            "Reopened zero-inclusive revolute joint cannot drive to its limit and return");

    // A joint attached to a non-component child is impossible through the API,
    // and must not be accepted from a corrupted or externally edited scene file.
    QJsonObject root=QJsonDocument::fromJson(readBytes(path)).object();
    QJsonArray items=root.value("parts").toArray();
    bool mutated=false;
    for(int i=0;i<items.size();++i){
        QJsonObject part=items[i].toObject();
        if(part.value("id").toString()!=child)continue;
        part.insert("component_definition_id",QString());
        part.insert("component_name",QString());
        part.insert("occurrence_id",QString());
        part.insert("placement_x",0.0);
        part.insert("placement_y",0.0);
        part.insert("placement_z",0.0);
        items[i]=part;mutated=true;
    }
    require(mutated,"Could not identify joint child in saved fixture");
    root.insert("parts",items);
    const QString malformed=dir.filePath("noncomponent-joint-child.cadscene");
    writeBytes(malformed,QJsonDocument(root).toJson());
    require(!reopened.load(malformed,&error)&&error.contains("not a component occurrence")&&
            reopened.find(child)&&reopened.find(child)->assemblyJointType=="Revolute"&&
            reopened.find(child)->componentDefinitionId==model.find(child)->componentDefinitionId,
            "Malformed saved joint child was accepted or failed to leave scene unchanged");
}

void testAssemblyOccurrencePoseCopyGuard() {
    QString error;
    cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),
            "Could not create occurrence-copy pose fixture");
    const QString root=doc.parts().first().id;
    require(doc.makeComponent(root,"Shared frame",&error)&&
            doc.insertComponentOccurrence(root,40,0,0,&error),
            "Could not create occurrence-copy fixture");
    const QString driven=doc.parts().last().id;
    require(doc.addRevoluteJoint(root,driven,0,0,0,0,0,1,-90,90,&error)&&
            doc.driveRevoluteJoint(driven,45,&error),
            "Could not pose revolute copy source");
    const QString token=doc.find(driven)->geometryToken;
    require(!doc.insertComponentOccurrence(driven,80,0,0,&error)&&
            error.contains("Reset revolute joints")&&doc.parts().size()==2,
            "Insertion from driven revolute geometry was accepted");
    require(!doc.duplicate(driven,5,0,0,&error)&&
            error.contains("Reset revolute joints")&&doc.parts().size()==2&&
            doc.find(driven)->geometryToken==token,
            "Duplication from driven revolute geometry was accepted or mutated the source");
    // Both rejected operations are history-neutral: the next undo reverses drive.
    require(doc.undo()&&doc.parts().size()==2&&
            doc.find(driven)->jointAngleDegrees==0,
            "Rejected occurrence copies changed the undo history");
    require(doc.redo()&&doc.find(driven)->jointAngleDegrees==45,
            "Redo after rejected occurrence copies lost the saved pose");
    require(doc.driveRevoluteJoint(driven,0,&error)&&
            doc.insertComponentOccurrence(driven,80,0,0,&error)&&
            doc.duplicate(driven,5,0,0,&error)&&doc.parts().size()==4,
            "Occurrence creation was rejected after returning revolute joint to zero");
    require(doc.parts()[2].assemblyJointType.isEmpty()&&
            doc.parts()[3].assemblyJointType.isEmpty(),
            "New occurrences unexpectedly inherited the source joint");

    // A rigid grandchild of a driven revolute joint has the same B-rep pose risk.
    cad::SceneDocument nested;
    require(nested.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create nested occurrence-copy fixture");
    const QString fixed=nested.parts().first().id;
    require(nested.makeComponent(fixed,"Fixed frame",&error)&&
            nested.insertComponentOccurrence(fixed,20,0,0,&error)&&
            nested.insertComponentOccurrence(fixed,40,0,0,&error),
            "Could not create nested occurrence-copy instances");
    const QString pivot=nested.parts()[1].id;
    const QString leaf=nested.parts()[2].id;
    require(nested.addRevoluteJoint(fixed,pivot,0,0,0,0,0,1,-90,90,&error)&&
            nested.addRigidJoint(pivot,leaf,&error)&&
            nested.driveRevoluteJoint(pivot,30,&error),
            "Could not pose nested occurrence-copy chain");
    require(!nested.insertComponentOccurrence(leaf,60,0,0,&error)&&
            !nested.duplicate(leaf,5,0,0,&error)&&nested.parts().size()==3,
            "Rigid descendant of driven revolute joint was copied");

    // A prismatic-only source has a representable XYZ placement and remains copyable.
    cad::SceneDocument slider;
    require(slider.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
            "Could not create prismatic copy fixture");
    const QString rail=slider.parts().first().id;
    require(slider.makeComponent(rail,"Rail",&error)&&
            slider.insertComponentOccurrence(rail,20,0,0,&error),
            "Could not create prismatic copy source");
    const QString carriage=slider.parts().last().id;
    require(slider.addPrismaticJoint(rail,carriage,1,0,0,-10,20,&error)&&
            slider.drivePrismaticJoint(carriage,12,&error)&&
            slider.insertComponentOccurrence(carriage,80,0,0,&error)&&
            slider.duplicate(carriage,8,0,0,&error),
            "Prismatic-only driven source could not be copied");
    require(slider.parts().size()==4&&slider.parts()[2].placementX==80&&
            slider.parts()[3].placementX==40,
            "Prismatic copied occurrences received incorrect saved placements");
}

void testAssemblySynchronizationRevoluteGuard() {
    QString error;
    cad::SceneDocument direct;
    require(direct.addPrimitive(cad::Primitive::Box,{10,20,30},&error),
            "Could not create revolute-sync fixture");
    const QString source=direct.parts().first().id;
    require(direct.makeComponent(source,"Shared bracket",&error)&&
            direct.insertComponentOccurrence(source,50,0,0,&error),
            "Could not create repeated component for revolute-sync guard");
    const QString child=direct.parts().last().id;
    require(direct.billOfMaterials(&error).size()==1&&
            direct.billOfMaterials(&error).first().quantity==2,
            "Matching component occurrences did not begin as one two-quantity BOM row");
    require(direct.addRevoluteJoint(source,child,50,0,0,0,0,1,-90,90,&error)&&
            direct.driveRevoluteJoint(child,90,&error),
            "Could not pose the repeated component's revolute joint");
    cad::BodyDiagnostics before;
    require(direct.diagnoseBody(child,&before,&error),"Could not inspect posed occurrence");
    const QString childToken=direct.find(child)->geometryToken;
    require(direct.scale(source,2,&error),"Could not edit the reusable source geometry");
    require(!direct.synchronizeComponentOccurrences(source,&error)&&
            error.contains("Reset revolute joints")&&
            direct.find(child)->geometryToken==childToken,
            "Synchronization overwrote a driven revolute occurrence");
    cad::BodyDiagnostics rejected;
    require(direct.diagnoseBody(child,&rejected,&error),"Could not re-inspect rejected occurrence");
    closeTo(rejected.volume,before.volume);
    closeTo(rejected.minX,before.minX);closeTo(rejected.minY,before.minY);
    closeTo(rejected.maxX,before.maxX);closeTo(rejected.maxY,before.maxY);
    require(!direct.synchronizeComponentOccurrences(child,&error)&&
            direct.find(child)->geometryToken==childToken,
            "Synchronization accepted a driven revolute occurrence as the source");
    // A failed sync must not push history; undo should revert the last geometry edit.
    require(direct.undo(),"Failed revolute synchronization changed the undo stack");
    cad::BodyDiagnostics undone;
    require(direct.diagnoseBody(source,&undone,&error),"Could not inspect source after undo");
    closeTo(undone.volume,before.volume);
    require(direct.driveRevoluteJoint(child,0,&error)&&direct.scale(source,2,&error)&&
            direct.synchronizeComponentOccurrences(source,&error),
            "Synchronization rejected a revolute occurrence returned to rest");
    cad::BodyDiagnostics synced;
    require(direct.diagnoseBody(child,&synced,&error),"Could not inspect rest-pose synchronization");
    closeTo(synced.volume,8*before.volume);
    const auto synchronizedBom=direct.billOfMaterials(&error);
    require(synchronizedBom.size()==1&&synchronizedBom.first().quantity==2,
            "Definition synchronization left matching occurrences split in the derived BOM");

    // The guarded occurrence may be a rigid descendant, rather than the
    // revolute child itself. Its orientation still follows the driven ancestor.
    cad::SceneDocument nested;
    require(nested.addPrimitive(cad::Primitive::Box,{10,20,30},&error),
            "Could not create nested-sync source");
    const QString nestedSource=nested.parts().first().id;
    require(nested.makeComponent(nestedSource,"Nested shared bracket",&error)&&
            nested.insertComponentOccurrence(nestedSource,60,0,0,&error),
            "Could not create nested-sync occurrence");
    const QString leaf=nested.parts().last().id;
    require(nested.addPrimitive(cad::Primitive::Box,{5,5,5},&error),
            "Could not create revolute ancestor fixture");
    const QString pivot=nested.parts().last().id;
    require(nested.makeComponent(pivot,"Rotating carrier",&error)&&
            nested.addRevoluteJoint(nestedSource,pivot,0,0,0,0,0,1,-90,90,&error)&&
            nested.addRigidJoint(pivot,leaf,&error)&&
            nested.driveRevoluteJoint(pivot,45,&error),
            "Could not build a revolute-to-rigid descendant chain");
    const QString leafToken=nested.find(leaf)->geometryToken;
    require(nested.scale(nestedSource,2,&error)&&
            !nested.synchronizeComponentOccurrences(nestedSource,&error)&&
            nested.find(leaf)->geometryToken==leafToken,
            "Synchronization discarded a rigid descendant's revolute pose");

    // Translational prismatic poses are expressible in the stored XYZ placement;
    // they must remain synchronizable.
    cad::SceneDocument slider;
    require(slider.addPrimitive(cad::Primitive::Box,{10,20,30},&error),
            "Could not create prismatic-sync fixture");
    const QString sliderSource=slider.parts().first().id;
    require(slider.makeComponent(sliderSource,"Sliding bracket",&error)&&
            slider.insertComponentOccurrence(sliderSource,30,0,0,&error),
            "Could not create prismatic-sync occurrence");
    const QString sliderChild=slider.parts().last().id;
    require(slider.addPrismaticJoint(sliderSource,sliderChild,1,0,0,-50,50,&error)&&
            slider.drivePrismaticJoint(sliderChild,10,&error)&&
            slider.scale(sliderSource,2,&error)&&
            slider.synchronizeComponentOccurrences(sliderSource,&error),
            "Synchronization incorrectly rejected a prismatic-only translation");
    cad::BodyDiagnostics sliderSynced;
    require(slider.diagnoseBody(sliderChild,&sliderSynced,&error),
            "Could not inspect synchronized prismatic occurrence");
    closeTo(sliderSynced.volume,48000);
    closeTo(slider.find(sliderChild)->placementX,40);
    closeTo(slider.find(sliderChild)->jointTranslationMm,10);
}

void testMotionInterferenceScan() {
    QString error;cad::SceneDocument doc;
    require(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create motion interference fixture");
    const QString root=doc.parts().first().id;
    require(doc.makeComponent(root,"Collision root",&error)&&doc.insertComponentOccurrence(root,20,0,0,&error),
            "Could not create motion interference occurrence");
    const QString child=doc.parts().last().id;
    require(doc.addPrismaticJoint(root,child,1,0,0,-15,1,&error)&&
            doc.setJointMotionKeyframes(child,{{0,0},{1,-15}},&error),
            ("Could not set up collision motion track: "+error.toStdString()).c_str());
    const QString originalToken=doc.find(child)->geometryToken;
    QVector<cad::MotionInterferenceSample> samples;
    require(doc.scanAssemblyMotionInterference(root,child,0,1,5,&samples,&error),
            ("Motion interference scan failed: "+error.toStdString()).c_str());
    require(samples.size()==5&&!samples[0].interfering&&!samples[1].interfering&&!samples[2].interfering&&
            samples[3].interfering&&samples[4].interfering&&
            std::abs(samples[3].overlapVolumeMm3-125)<1e-4&&
            std::abs(samples[4].overlapVolumeMm3-500)<1e-4,
            "Motion scan missed sampled collisions or reported wrong overlap volumes");
    require(doc.find(child)->geometryToken==originalToken&&doc.find(child)->jointTranslationMm==0,
            "Read-only motion interference scan changed the assembly pose");
    const auto validSamples=samples;
    require(!doc.scanAssemblyMotionInterference(root,child,1,0,5,&samples,&error)&&samples==validSamples,
            "Invalid motion scan changed its output");
    require(!doc.scanAssemblyMotionInterference(root,child,0,1,1,&samples,&error)&&samples==validSamples,
            "One-sample motion scan was accepted");
}

void testAssemblyJoints() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Assembly-joint temporary folder failed");
    cad::SceneDocument rigid;
    require(rigid.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create rigid-joint source component");
    const QString parent=rigid.parts().first().id;
    require(rigid.makeComponent(parent,"Joint block",&error)&&rigid.insertComponentOccurrence(parent,25,0,0,&error),
            "Could not create fixed component occurrences for a rigid joint");
    const QString child=rigid.parts().last().id;
    require(!rigid.addRigidJoint(parent,parent,&error)&&!rigid.addRigidJoint("missing",child,&error),
            "Rigid joint accepted a duplicate or missing occurrence reference");
    require(rigid.addRigidJoint(parent,child,&error),"Could not connect the two occurrences with a rigid joint");
    require(rigid.parts().last().assemblyJointType=="Rigid"&&rigid.parts().last().assemblyJointParentBodyId==parent,
            "Rigid joint parent/child relationship was not recorded");
    require(!rigid.addRevoluteJoint(parent,child,0,0,0,0,0,1,-90,90,&error),"A child occurrence accepted two joints");
    require(rigid.undo()&&rigid.parts().last().assemblyJointType.isEmpty(),"Rigid joint creation did not undo cleanly");
    require(rigid.redo()&&rigid.parts().last().assemblyJointType=="Rigid","Rigid joint creation could not be redone");
    require(rigid.translate(parent,5,0,0,&error),"Rigid parent occurrence could not carry its jointed child");
    closeTo(rigid.parts().first().placementX,5);closeTo(rigid.parts().last().placementX,30);
    require(rigid.undo()&&rigid.parts().first().placementX==0&&rigid.parts().last().placementX==25,
            "Rigid subassembly translation was not undone as one operation");
    require(rigid.redo()&&rigid.parts().first().placementX==5&&rigid.parts().last().placementX==30,
            "Rigid subassembly translation was not redone as one operation");
    require(rigid.rotate(parent,cad::Axis::Z,90,&error),"Rigid parent rotation could not carry its jointed child");
    closeTo(rigid.parts().first().placementX,0);closeTo(rigid.parts().first().placementY,5);
    closeTo(rigid.parts().last().placementX,0);closeTo(rigid.parts().last().placementY,30);
    require(rigid.undo()&&std::abs(rigid.parts().first().placementX-5)<1e-8&&
            std::abs(rigid.parts().last().placementX-30)<1e-8,
            "Rigid subassembly rotation was not undone as one operation");
    require(rigid.redo()&&std::abs(rigid.parts().last().placementY-30)<1e-8,
            "Rigid subassembly rotation was not redone as one operation");

    cad::SceneDocument nested;
    require(nested.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create nested-joint root component");
    const QString rootId=nested.parts().first().id;
    require(nested.makeComponent(rootId,"Nested root",&error)&&nested.insertComponentOccurrence(rootId,10,0,0,&error),
            "Could not create nested-joint middle occurrence");
    const QString middle=nested.parts().last().id;
    require(nested.insertComponentOccurrence(middle,20,0,0,&error),"Could not create nested-joint leaf occurrence");
    const QString leaf=nested.parts().last().id;
    require(nested.addRigidJoint(rootId,middle,&error)&&nested.addRigidJoint(middle,leaf,&error),
            "Nested rigid-joint chain could not be created");
    require(nested.translate(rootId,5,0,0,&error),"Nested root translation failed");
    closeTo(nested.find(rootId)->placementX,5);closeTo(nested.find(middle)->placementX,15);closeTo(nested.find(leaf)->placementX,25);
    require(nested.rotate(rootId,cad::Axis::Z,90,&error),"Nested root rotation failed");
    closeTo(nested.find(middle)->placementY,15);closeTo(nested.find(leaf)->placementY,25);
    require(nested.undo()&&std::abs(nested.find(leaf)->placementX-25)<1e-8&&
            std::abs(nested.find(leaf)->placementY)<1e-8,
            "Nested root rotation undo did not restore the complete chain");
    require(nested.undo()&&std::abs(nested.find(leaf)->placementX-20)<1e-8,
            "Nested root translation undo did not restore the complete chain");
    require(nested.redo()&&nested.redo()&&std::abs(nested.find(leaf)->placementY-25)<1e-8,
            "Nested assembly transforms could not be redone through the entire chain");

    cad::SceneDocument revolute;
    require(revolute.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create revolute-joint base component");
    const QString fixed=revolute.parts().first().id;
    require(revolute.makeComponent(fixed,"Pivot base",&error)&&revolute.insertComponentOccurrence(fixed,20,0,0,&error),
            "Could not create occurrences for a revolute joint");
    const QString moving=revolute.parts().last().id;
    require(revolute.insertComponentOccurrence(fixed,40,0,0,&error),"Could not create a rigid revolute subassembly descendant");
    const QString revoluteLeaf=revolute.parts().last().id;
    require(revolute.addRigidJoint(moving,revoluteLeaf,&error),"Could not attach a rigid descendant to the revolute component");
    require(!revolute.addRevoluteJoint(fixed,moving,0,0,0,0,0,0,-90,90,&error),"Zero-length revolute axis was accepted");
    require(!revolute.addRevoluteJoint(fixed,moving,0,0,0,0,0,1,90,-90,&error),"Reversed revolute limits were accepted");
    require(revolute.addRevoluteJoint(fixed,moving,0,0,0,0,0,5,-90,90,&error),"Valid Z-axis revolute joint was rejected");
    closeTo(revolute.find(moving)->jointAxisZ,1.0);
    require(!revolute.driveRevoluteJoint(moving,91,&error),"Revolute joint exceeded its saved angle limits");
    require(revolute.driveRevoluteJoint(moving,90,&error),"Could not drive revolute joint to 90 degrees");
    closeTo(revolute.find(moving)->jointAngleDegrees,90);closeTo(revolute.find(moving)->placementX,0,1e-8);closeTo(revolute.find(moving)->placementY,20,1e-8);
    closeTo(revolute.find(revoluteLeaf)->placementX,0,1e-8);closeTo(revolute.find(revoluteLeaf)->placementY,40,1e-8);
    cad::BodyDiagnostics geometry;require(revolute.diagnoseBody(moving,&geometry,&error),"Rotated occurrence geometry could not be inspected");
    closeTo(geometry.volume,1000);require(geometry.valid,"Revolute motion produced invalid geometry");
    require(revolute.undo()&&std::abs(revolute.find(moving)->jointAngleDegrees)<1e-8&&
            std::abs(revolute.find(revoluteLeaf)->placementX-40)<1e-8,"Driving a revolute subassembly was not undoable");
    require(revolute.redo()&&std::abs(revolute.find(moving)->jointAngleDegrees-90)<1e-8&&
            std::abs(revolute.find(revoluteLeaf)->placementY-40)<1e-8,"Driving a revolute subassembly could not be redone");
    require(revolute.translate(fixed,10,0,0,&error),"Revolute parent could not carry its jointed child");
    closeTo(revolute.parts().first().placementX,10);closeTo(revolute.find(moving)->placementX,10);
    closeTo(revolute.find(moving)->jointOriginX,10);
    require(revolute.undo()&&std::abs(revolute.find(moving)->placementX)<1e-8&&
            std::abs(revolute.find(moving)->jointOriginX)<1e-8,
            "Revolute subassembly translation did not restore its joint axis on undo");
    require(revolute.redo()&&std::abs(revolute.find(moving)->placementX-10)<1e-8&&
            std::abs(revolute.find(moving)->jointOriginX-10)<1e-8,
            "Revolute subassembly translation did not carry its joint axis on redo");
    require(revolute.rotate(fixed,cad::Axis::X,90,&error),"Revolute parent rotation could not carry its jointed child");
    closeTo(revolute.find(moving)->jointAxisY,-1);closeTo(revolute.find(moving)->jointAxisZ,0);
    closeTo(revolute.find(moving)->placementY,0);closeTo(revolute.find(moving)->placementZ,20);
    require(revolute.undo()&&std::abs(revolute.find(moving)->jointAxisZ-1)<1e-8&&
            std::abs(revolute.find(moving)->placementY-20)<1e-8,
            "Revolute subassembly rotation did not restore joint frame on undo");
    require(revolute.redo()&&std::abs(revolute.find(moving)->jointAxisY+1)<1e-8&&
            std::abs(revolute.find(moving)->placementZ-20)<1e-8,
            "Revolute subassembly rotation did not transform joint frame on redo");
    const QString path=dir.filePath("revolute.cadscene");require(revolute.saveAs(path,&error),"Could not save a driven revolute joint");
    cad::SceneDocument loaded;require(loaded.load(path,&error),"Could not load a saved revolute joint");
    require(loaded.find(moving)->assemblyJointType=="Revolute"&&loaded.find(moving)->assemblyJointParentBodyId==fixed&&
            loaded.find(moving)->jointAngleDegrees==90&&loaded.find(moving)->jointMaximumDegrees==90,
            "Revolute joint identity, limits, or state did not persist");
    const QByteArray saved=readBytes(path);QJsonObject root=QJsonDocument::fromJson(saved).object();QJsonArray parts=root.value("parts").toArray();
    QJsonObject childObject=parts[1].toObject();QJsonObject joint=childObject.value("assembly_joint").toObject();joint.insert("parent_body_id","missing-parent");
    childObject.insert("assembly_joint",joint);parts[1]=childObject;root.insert("parts",parts);
    const QString corrupt=dir.filePath("bad-joint.cadscene");writeBytes(corrupt,QJsonDocument(root).toJson());
    require(!loaded.load(corrupt,&error)&&loaded.parts().size()==3&&loaded.find(moving)->jointAngleDegrees==90,
            "Invalid joint references partially replaced the current scene");

    cad::SceneDocument slider;
    require(slider.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create prismatic-joint source component");
    const QString sliderParent=slider.parts().first().id;
    require(slider.makeComponent(sliderParent,"Slider base",&error)&&slider.insertComponentOccurrence(sliderParent,20,0,0,&error),
            "Could not create occurrences for a prismatic joint");
    const QString sliderChild=slider.parts().last().id;
    require(!slider.addPrismaticJoint(sliderParent,sliderChild,0,0,0,-10,10,&error),"Zero-length prismatic axis was accepted");
    require(!slider.addPrismaticJoint(sliderParent,sliderChild,1,0,0,0,10,&error),"Prismatic travel limits excluding zero were accepted");
    require(slider.addPrismaticJoint(sliderParent,sliderChild,2,0,0,-10,25,&error),"Valid X-axis prismatic joint was rejected");
    closeTo(slider.find(sliderChild)->jointAxisX,1);closeTo(slider.find(sliderChild)->jointMinimumMm,-10);
    const QString sliderLeafSource=slider.parts().first().id;
    require(slider.insertComponentOccurrence(sliderLeafSource,50,0,0,&error),"Could not create prismatic subassembly leaf occurrence");
    const QString sliderLeaf=slider.parts().last().id;
    require(slider.addRigidJoint(sliderChild,sliderLeaf,&error),"Could not attach a rigid descendant to the prismatic carriage");
    require(!slider.drivePrismaticJoint(sliderChild,26,&error)&&std::abs(slider.find(sliderChild)->placementX-20)<1e-9,
            "Prismatic drive exceeded its travel or moved the child before validation");
    require(slider.drivePrismaticJoint(sliderChild,12,&error),"Could not drive the prismatic joint within travel limits");
    closeTo(slider.find(sliderChild)->placementX,32);closeTo(slider.find(sliderLeaf)->placementX,62);
    closeTo(slider.find(sliderChild)->jointTranslationMm,12);
    require(slider.undo()&&std::abs(slider.find(sliderChild)->placementX-20)<1e-9&&
            std::abs(slider.find(sliderLeaf)->placementX-50)<1e-9&&std::abs(slider.find(sliderChild)->jointTranslationMm)<1e-9,
            "Prismatic subassembly drive did not undo atomically");
    require(slider.redo()&&std::abs(slider.find(sliderLeaf)->placementX-62)<1e-9&&
            std::abs(slider.find(sliderChild)->jointTranslationMm-12)<1e-9,"Prismatic subassembly drive could not be redone");
    const QVector<cad::JointMotionKeyframe> sliderKeys={{0,0},{2,10},{4,0}};
    require(slider.setJointMotionKeyframes(sliderChild,sliderKeys,&error),"Could not save a prismatic motion-study track");
    require(!slider.setJointMotionKeyframes(sliderChild,{{0,0},{0,10}},&error)&&
            !slider.setJointMotionKeyframes(sliderChild,{{0,0},{1,30}},&error),
            "Invalid or out-of-limit motion-study keyframes were accepted");
    require(slider.undo()&&slider.find(sliderChild)->jointMotionKeyframes.isEmpty()&&slider.redo()&&
            slider.find(sliderChild)->jointMotionKeyframes.size()==3,"Motion-track editing was not undoable and redoable");
    const QString sliderPath=dir.filePath("prismatic.cadscene");require(slider.saveAs(sliderPath,&error),"Could not save a prismatic joint");
    cad::SceneDocument sliderLoaded;require(sliderLoaded.load(sliderPath,&error),"Could not load a saved prismatic joint");
    require(sliderLoaded.find(sliderChild)->assemblyJointType=="Prismatic"&&
            sliderLoaded.find(sliderChild)->assemblyJointParentBodyId==sliderParent&&
            sliderLoaded.find(sliderChild)->jointTranslationMm==12&&sliderLoaded.find(sliderChild)->jointMaximumMm==25&&
            sliderLoaded.find(sliderChild)->jointMotionKeyframes.size()==3,
            "Prismatic joint identity, travel, or driven position did not persist");
    require(sliderLoaded.translate(sliderParent,3,0,0,&error),"Prismatic parent could not carry its jointed subassembly");
    closeTo(sliderLoaded.find(sliderChild)->jointOriginX,3);closeTo(sliderLoaded.find(sliderChild)->placementX,35);
    require(sliderLoaded.drivePrismaticJoint(sliderChild,0,&error),"Prismatic joint could not return to its zero position");
    closeTo(sliderLoaded.find(sliderChild)->placementX,23);closeTo(sliderLoaded.find(sliderLeaf)->placementX,53);
    require(sliderLoaded.evaluateAssemblyMotionStudy(1.0,&error),"Could not evaluate an interpolated prismatic motion-study frame");
    closeTo(sliderLoaded.find(sliderChild)->jointTranslationMm,5);closeTo(sliderLoaded.find(sliderChild)->placementX,28);
    closeTo(sliderLoaded.find(sliderLeaf)->placementX,58);
    require(!sliderLoaded.evaluateAssemblyMotionStudy(3601,&error),"Out-of-range motion-study time was accepted");

    cad::SceneDocument pose;
    require(pose.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create assembly-pose source component");
    const QString poseRoot=pose.parts().first().id;
    require(pose.makeComponent(poseRoot,"Pose root",&error)&&pose.insertComponentOccurrence(poseRoot,20,0,0,&error),
            "Could not create a revolute child for assembly-pose solving");
    const QString poseRevolute=pose.parts().last().id;
    require(pose.insertComponentOccurrence(poseRoot,40,0,0,&error),"Could not create a prismatic descendant for assembly-pose solving");
    const QString poseSlider=pose.parts().last().id;
    require(pose.addRevoluteJoint(poseRoot,poseRevolute,0,0,5,0,0,1,-90,90,&error)&&
            pose.addPrismaticJoint(poseRevolute,poseSlider,1,0,0,-20,20,&error),
            "Could not create the mixed revolute/prismatic assembly chain");
    QMap<QString,double> combinedPose{{poseRevolute,90.0},{poseSlider,10.0}};
    require(!pose.solveAssemblyPose({{poseRevolute,45.0},{poseSlider,50.0}},&error)&&
            std::abs(pose.find(poseRevolute)->jointAngleDegrees)<1e-9&&std::abs(pose.find(poseSlider)->jointTranslationMm)<1e-9,
            "Invalid mixed-joint pose partially changed the assembly");
    require(pose.solveAssemblyPose(combinedPose,&error),"Mixed-joint parent-first pose solve failed");
    closeTo(pose.find(poseRevolute)->jointAngleDegrees,90);closeTo(pose.find(poseSlider)->jointTranslationMm,10);
    closeTo(pose.find(poseSlider)->placementX,0,1e-8);closeTo(pose.find(poseSlider)->placementY,50,1e-8);
    require(pose.undo()&&std::abs(pose.find(poseRevolute)->jointAngleDegrees)<1e-9&&
            std::abs(pose.find(poseSlider)->jointTranslationMm)<1e-9&&std::abs(pose.find(poseSlider)->placementX-40)<1e-8,
            "Mixed-joint pose was not one atomic undo step");
    require(pose.redo()&&std::abs(pose.find(poseSlider)->placementY-50)<1e-8,
            "Mixed-joint pose could not be redone");

    cad::SceneDocument definition;
    require(definition.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Could not create reusable-definition source");
    const QString definitionSource=definition.parts().first().id;
    require(definition.makeComponent(definitionSource,"Shared bracket",&error)&&
            definition.insertComponentOccurrence(definitionSource,30,0,0,&error),"Could not create reusable-definition occurrence");
    const QString definitionOccurrence=definition.parts().last().id;
    require(definition.setMaterial(definitionSource,"Aluminum 6061",2700,&error)&&
            definition.setMaterialEngineering({definitionSource},"Supplier data sheet",69.0,&error),
            "Could not edit reusable-definition material metadata");
    require(definition.setPartMetadata(definitionSource,"BRK-204","Adjustable mounting bracket","C","Brackets",
                                       "Purchased","Example Supply","BRK-204-A",&error),
            "Could not edit reusable-definition BOM metadata");
    require(definition.setVisibility(definitionOccurrence,false,&error),
            "Could not set occurrence-local visibility before definition synchronization");
    require(definition.scale(definitionSource,2,&error),"Could not edit the reusable component source geometry");
    cad::BodyDiagnostics beforeSync;require(definition.diagnoseBody(definitionOccurrence,&beforeSync,&error),"Could not inspect an occurrence before definition sync");
    closeTo(beforeSync.volume,1000);
    require(definition.synchronizeComponentOccurrences(definitionSource,&error),"Could not synchronize edited component geometry to its reusable occurrence");
    cad::BodyDiagnostics afterSync;require(definition.diagnoseBody(definitionOccurrence,&afterSync,&error),"Could not inspect an occurrence after definition sync");
    closeTo(afterSync.volume,8000);closeTo(definition.find(definitionOccurrence)->placementX,30);
    require(definition.find(definitionOccurrence)->partNumber=="BRK-204"&&
            definition.find(definitionOccurrence)->description=="Adjustable mounting bracket"&&
            definition.find(definitionOccurrence)->revision=="C"&&
            definition.find(definitionOccurrence)->procurementType=="Purchased"&&
            definition.find(definitionOccurrence)->supplier=="Example Supply"&&
            definition.find(definitionOccurrence)->materialName=="Aluminum 6061"&&
            definition.find(definitionOccurrence)->materialEngineering.youngsModulusGPa==69.0&&
            !definition.find(definitionOccurrence)->visible&&definition.find(definitionOccurrence)->placementX==30.0,
            "Reusable-definition synchronization left occurrence BOM or material data stale");
    require(definition.undo()&&definition.diagnoseBody(definitionOccurrence,&beforeSync,&error),"Component-definition synchronization did not undo");
    closeTo(beforeSync.volume,1000);
    require(definition.find(definitionOccurrence)->partNumber.isEmpty()&&definition.find(definitionOccurrence)->materialName.isEmpty(),
            "Undoing definition synchronization did not restore occurrence metadata");
    require(definition.redo()&&definition.diagnoseBody(definitionOccurrence,&afterSync,&error),"Component-definition synchronization did not redo");
    closeTo(afterSync.volume,8000);
    require(definition.find(definitionOccurrence)->partNumber=="BRK-204"&&definition.find(definitionOccurrence)->materialName=="Aluminum 6061",
            "Redoing definition synchronization did not restore occurrence metadata");
}
void testSurfaceBodyPersistence() {
    QString error;QTemporaryDir dir;require(dir.isValid(),"Surface-body test folder failed");
    BRepBuilderAPI_MakePolygon boundary;
    boundary.Add(gp_Pnt(0,0,0));boundary.Add(gp_Pnt(10,0,0));
    boundary.Add(gp_Pnt(10,5,0));boundary.Add(gp_Pnt(0,5,0));boundary.Close();
    const TopoDS_Shape surface=BRepBuilderAPI_MakeFace(boundary.Wire()).Face();
    require(BRepCheck_Analyzer(surface).IsValid(),"Surface-body fixture is invalid");

    cad::SceneDocument document;
    require(!document.addSurfaceBody("Invalid solid",BRepPrimAPI_MakeBox(5,5,5).Shape(),&error)&&
            document.parts().isEmpty()&&!document.canUndo(),
            "Solid input was accepted as a surface body or changed the document");
    require(!document.addSurfaceBody("Null surface",TopoDS_Shape{},&error)&&document.parts().isEmpty(),
            "Null surface input was accepted or changed the document");
    error.clear();require(document.addSurfaceBody("Reference Surface",surface,&error),
            ("Could not add a persisted surface body: "+error.toStdString()).c_str());
    const QString id=document.parts().first().id;
    require(document.parts().size()==1&&document.parts().first().isSurfaceBody&&
            document.parts().first().shape.ShapeType()==TopAbs_FACE,
            "Surface body type or geometry was not retained in the scene record");
    cad::HoleCommand invalidHole;
    require(!document.prepareHole(id,5,2.5,1,2,5,false,&invalidHole,&error)&&
            error.contains("requires a solid")&&document.parts().size()==1,
            "A solid-only hole operation did not reject the surface body cleanly");
    require(document.translate(id,7,3,2,&error),"Surface body translation failed");
    Bnd_Box movedBounds;BRepBndLib::Add(document.find(id)->shape,movedBounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;movedBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    closeTo(xmin,7);closeTo(ymin,3);closeTo(zmin,2);
    require(document.undo(),"Surface body translation could not be undone");
    Bnd_Box originalBounds;BRepBndLib::Add(document.find(id)->shape,originalBounds);
    originalBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);closeTo(xmin,0);closeTo(ymin,0);closeTo(zmin,0);
    require(document.redo(),"Surface body translation could not be redone");
    require(document.rotate(id,cad::Axis::Z,90,&error),"Surface body rotation failed");
    require(document.rename(id,"Trim guide",&error)&&document.setVisibility(id,false,&error),
            "Surface body could not be renamed or hidden");

    cad::SceneDocument thickening;
    require(thickening.addSurfaceBody("Plate Surface",surface,&error),"Could not create a surface-thickening fixture");
    const QString thinSurfaceId=thickening.parts().first().id;
    require(!thickening.thickenSurfaceBody(thinSurfaceId,0,&error)&&thickening.parts().size()==1,
            "Zero thickness was accepted or changed the scene");
    require(thickening.thickenSurfaceBody(thinSurfaceId,2,&error)&&thickening.parts().size()==2&&
            !thickening.parts().last().isSurfaceBody,"Could not create a solid from a planar surface body");
    closeTo(volume(thickening.parts().last().shape),100.0,1e-5);
    require(thickening.undo()&&thickening.parts().size()==1&&thickening.parts().first().isSurfaceBody&&
            thickening.redo()&&thickening.parts().size()==2,
            "Surface thickening did not preserve the source or participate in undo/redo");

    cad::SceneDocument stitching;
    const TopoDS_Shape box=BRepPrimAPI_MakeBox(10,10,10).Shape();QStringList faceIds;
    int faceIndex=0;
    for(TopExp_Explorer faces(box,TopAbs_FACE);faces.More();faces.Next()){
        const QString name=QStringLiteral("Box face %1").arg(++faceIndex);
        require(stitching.addSurfaceBody(name,faces.Current(),&error),"Could not create a sewing face fixture");
        faceIds.append(stitching.parts().last().id);
    }
    require(faceIds.size()==6&&stitching.stitchSurfaceBodies(faceIds,1e-4,&error)&&
            stitching.parts().size()==7&&!stitching.parts().last().isSurfaceBody,
            ("Six shared-boundary faces did not stitch into a solid: "+error.toStdString()).c_str());
    closeTo(volume(stitching.parts().last().shape),1000.0,1e-5);
    require(stitching.undo()&&stitching.parts().size()==6&&stitching.redo()&&stitching.parts().size()==7,
            "Surface stitching did not preserve inputs or participate in undo/redo");

    BRepBuilderAPI_MakePolygon firstShellFace;
    firstShellFace.Add(gp_Pnt(0,0,0));firstShellFace.Add(gp_Pnt(10,0,0));
    firstShellFace.Add(gp_Pnt(10,5,0));firstShellFace.Add(gp_Pnt(0,5,0));firstShellFace.Close();
    BRepBuilderAPI_MakePolygon secondShellFace;
    secondShellFace.Add(gp_Pnt(0,0,0));secondShellFace.Add(gp_Pnt(10,0,0));
    secondShellFace.Add(gp_Pnt(10,0,5));secondShellFace.Add(gp_Pnt(0,0,5));secondShellFace.Close();
    cad::SceneDocument shellMerge;
    require(shellMerge.addSurfaceBody("Shell base",BRepBuilderAPI_MakeFace(firstShellFace.Wire()).Face(),&error)&&
            shellMerge.addSurfaceBody("Shell side",BRepBuilderAPI_MakeFace(secondShellFace.Wire()).Face(),&error),
            "Could not create adjacent surface faces for sewing");
    const QStringList shellSourceIds{shellMerge.parts()[0].id,shellMerge.parts()[1].id};
    require(!shellMerge.sewSurfaceBodies(shellSourceIds,0,&error)&&shellMerge.parts().size()==2,
            "Invalid surface sewing tolerance changed the scene");
    error.clear();
    require(shellMerge.sewSurfaceBodies(shellSourceIds,1e-4,&error)&&shellMerge.parts().size()==3&&
            shellMerge.parts().last().isSurfaceBody&&shellMerge.parts().last().shape.ShapeType()==TopAbs_SHELL,
            ("Adjacent surface bodies did not sew into one open shell: "+error.toStdString()).c_str());
    int sewnFaceCount=0;
    for(TopExp_Explorer faces(shellMerge.parts().last().shape,TopAbs_FACE);faces.More();faces.Next())++sewnFaceCount;
    require(sewnFaceCount==2&&BRepCheck_Analyzer(shellMerge.parts().last().shape).IsValid(),
            "Sewn shell lost an input face or is topologically invalid");
    require(shellMerge.parts()[0].isSurfaceBody&&shellMerge.parts()[1].isSurfaceBody&&
            shellMerge.undo()&&shellMerge.parts().size()==2&&shellMerge.redo()&&shellMerge.parts().size()==3,
            "Surface-shell sewing did not preserve the inputs or commit as one undoable edit");
    const QString sewnShellId=shellMerge.parts().last().id;
    require(shellMerge.addPrimitive(cad::Primitive::Box,{2,7,7},&error),
            "Could not create the multi-face shell-split tool");
    const QString shellSplitToolId=shellMerge.parts().last().id;
    require(shellMerge.translate(shellSplitToolId,4,-1,-1,&error),
            "Could not position the multi-face shell-split tool");
    const int beforeShellSplit=shellMerge.parts().size();
    error.clear();
    const bool shellSplitSucceeded=shellMerge.splitSurfaceBodyBySolid(sewnShellId,shellSplitToolId,&error);
    require(shellSplitSucceeded,
            ("Solid-guided splitting rejected a sewn multi-face shell: "+error.toStdString()).c_str());
    double shellSplitArea=0.0;int shellSplitCount=0;
    for(int index=beforeShellSplit;index<shellMerge.parts().size();++index){
        require(shellMerge.parts()[index].isSurfaceBody,
                "A multi-face split fragment was persisted as a solid body");
        require(shellMerge.parts()[index].shape.ShapeType()==TopAbs_FACE,
                "A multi-face split fragment was not persisted as an independently selectable face");
        GProp_GProps area;BRepGProp::SurfaceProperties(shellMerge.parts()[index].shape,area);
        shellSplitArea+=area.Mass();++shellSplitCount;
    }
    require(shellSplitCount==6,"A solid did not split both faces of the sewn shell into six face fragments");
    closeTo(shellSplitArea,100.0,1e-5);
    require(shellMerge.undo()&&shellMerge.parts().size()==beforeShellSplit&&
            shellMerge.redo()&&shellMerge.parts().size()==beforeShellSplit+shellSplitCount,
            "Multi-face shell splitting was not one undoable operation");

    BRepBuilderAPI_MakePolygon trimBoundary;
    trimBoundary.Add(gp_Pnt(0,0,1));trimBoundary.Add(gp_Pnt(10,0,1));
    trimBoundary.Add(gp_Pnt(10,5,1));trimBoundary.Add(gp_Pnt(0,5,1));trimBoundary.Close();
    const TopoDS_Shape trimSurface=BRepBuilderAPI_MakeFace(trimBoundary.Wire()).Face();
    cad::SceneDocument insideTrim;
    require(insideTrim.addSurfaceBody("Trim Source",trimSurface,&error)&&
            insideTrim.addPrimitive(cad::Primitive::Box,{4,7,2},&error),
            "Could not create a surface trimming fixture");
    const QString trimSourceId=insideTrim.parts()[0].id;
    const QString trimToolId=insideTrim.parts()[1].id;
    require(insideTrim.translate(trimToolId,0,-1,0,&error),"Could not position the surface trimming tool");
    const int beforeInsideTrim=insideTrim.parts().size();
    require(!insideTrim.trimSurfaceBody(trimSourceId,trimSourceId,true,&error)&&
            insideTrim.parts().size()==beforeInsideTrim,"A surface was accepted as its own solid trimming tool");
    error.clear();
    require(insideTrim.trimSurfaceBody(trimSourceId,trimToolId,true,&error)&&
            insideTrim.parts().size()==beforeInsideTrim+1&&insideTrim.parts().last().isSurfaceBody,
            ("Keeping the surface portion inside a solid failed: "+error.toStdString()).c_str());
    GProp_GProps insideTrimArea;BRepGProp::SurfaceProperties(insideTrim.parts().last().shape,insideTrimArea);
    closeTo(insideTrimArea.Mass(),20.0,1e-5);
    require(insideTrim.parts()[0].isSurfaceBody&&insideTrim.parts()[1].id==trimToolId&&
            insideTrim.undo()&&insideTrim.parts().size()==beforeInsideTrim&&
            insideTrim.redo()&&insideTrim.parts().size()==beforeInsideTrim+1,
            "Surface trimming did not preserve its sources or participate in undo/redo");

    cad::SceneDocument outsideTrim;
    require(outsideTrim.addSurfaceBody("Split Trim Source",trimSurface,&error)&&
            outsideTrim.addPrimitive(cad::Primitive::Box,{2,7,2},&error),
            "Could not create a multi-piece surface trimming fixture");
    const QString outsideSourceId=outsideTrim.parts()[0].id;
    const QString outsideToolId=outsideTrim.parts()[1].id;
    require(outsideTrim.translate(outsideToolId,4,-1,0,&error),"Could not position the central trim tool");
    const int beforeOutsideTrim=outsideTrim.parts().size();
    require(outsideTrim.trimSurfaceBody(outsideSourceId,outsideToolId,false,&error)&&
            outsideTrim.parts().size()==beforeOutsideTrim+2,
            ("Keeping the outside produced the wrong number of disconnected surface bodies: "+error.toStdString()).c_str());
    double outsideArea=0.0;
    for(int i=beforeOutsideTrim;i<outsideTrim.parts().size();++i){
        require(outsideTrim.parts()[i].isSurfaceBody,"A trimmed surface component was stored as a solid");
        GProp_GProps pieceArea;BRepGProp::SurfaceProperties(outsideTrim.parts()[i].shape,pieceArea);
        outsideArea+=pieceArea.Mass();
    }
    closeTo(outsideArea,40.0,1e-5);
    require(outsideTrim.undo()&&outsideTrim.parts().size()==beforeOutsideTrim&&
            outsideTrim.redo()&&outsideTrim.parts().size()==beforeOutsideTrim+2,
            "Multi-piece surface trimming did not undo and redo atomically");

    cad::SceneDocument splitSurface;
    require(splitSurface.addSurfaceBody("Split Plane",trimSurface,&error)&&
            splitSurface.addPrimitive(cad::Primitive::Box,{2,7,2},&error),
            "Could not create a solid-guided surface-split fixture");
    const QString splitSourceId=splitSurface.parts()[0].id;
    const QString splitToolId=splitSurface.parts()[1].id;
    require(splitSurface.translate(splitToolId,4,-1,0,&error),"Could not position the surface-split tool");
    const int beforeSplit=splitSurface.parts().size();
    require(splitSurface.splitSurfaceBodyBySolid(splitSourceId,splitToolId,&error)&&
            splitSurface.parts().size()==beforeSplit+3,
            ("Solid-guided splitting did not create the expected three surface fragments: "+error.toStdString()).c_str());
    double splitArea=0.0;
    for(int i=beforeSplit;i<splitSurface.parts().size();++i){
        require(splitSurface.parts()[i].isSurfaceBody&&splitSurface.parts()[i].shape.ShapeType()==TopAbs_FACE,
                "A split fragment was not persisted as a separate surface body");
        GProp_GProps fragmentArea;BRepGProp::SurfaceProperties(splitSurface.parts()[i].shape,fragmentArea);
        splitArea+=fragmentArea.Mass();
    }
    closeTo(splitArea,50.0,1e-5);
    require(splitSurface.parts()[0].id==splitSourceId&&splitSurface.parts()[1].id==splitToolId&&
            splitSurface.undo()&&splitSurface.parts().size()==beforeSplit&&
            splitSurface.redo()&&splitSurface.parts().size()==beforeSplit+3,
            "Surface splitting did not preserve the source/tool or use one undoable edit");

    cad::SceneDocument unchangedSplit;
    require(unchangedSplit.addSurfaceBody("Unchanged Plane",trimSurface,&error)&&
            unchangedSplit.addPrimitive(cad::Primitive::Box,{2,2,2},&error),
            "Could not create a no-intersection surface-split fixture");
    const QString unchangedSurfaceId=unchangedSplit.parts()[0].id;
    const QString unchangedToolId=unchangedSplit.parts()[1].id;
    require(unchangedSplit.translate(unchangedToolId,20,20,0,&error),"Could not place the no-intersection split tool");
    const int unchangedCount=unchangedSplit.parts().size();
    error.clear();
    require(!unchangedSplit.splitSurfaceBodyBySolid(unchangedSurfaceId,unchangedToolId,&error)&&
            unchangedSplit.parts().size()==unchangedCount,
            "A non-intersecting solid was accepted as a surface split or changed the scene");

    const QString scenePath=dir.filePath("surface-body.cadscene");
    require(document.saveAs(scenePath,&error),"Surface body scene could not be saved");
    cad::SceneDocument loaded;
    require(loaded.load(scenePath,&error),("Surface body scene could not be reopened: "+error.toStdString()).c_str());
    require(loaded.parts().size()==1&&loaded.parts().first().id==id&&loaded.parts().first().name=="Trim guide"&&
            loaded.parts().first().isSurfaceBody&&!loaded.parts().first().visible&&
            loaded.parts().first().shape.ShapeType()==TopAbs_FACE&&BRepCheck_Analyzer(loaded.parts().first().shape).IsValid(),
            "Scene round trip lost surface-body type, identity, name, visibility, or valid geometry");
}
} // namespace
int main(int argc,char** argv) {
    QGuiApplication application(argc,argv);
    try {
        if(argc==2&&std::string(argv[1])=="--wing-rib") { testWingRib(); std::cout<<"PASS: "<<checks<<" Wing rib assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--modeling-tools") { testModelingTools(); std::cout<<"PASS: "<<checks<<" assertions across "<<static_cast<int>(cad::ModelingTool::Count)<<" modeling tools\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--sweeps") { testSweeps(); std::cout<<"PASS: "<<checks<<" sweep assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--elliptical-loft") { testEllipticalLoft(); std::cout<<"PASS: "<<checks<<" elliptical-loft assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--nonuniform-scale") { testNonUniformScale(); std::cout<<"PASS: "<<checks<<" nonuniform-scale assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--custom-axis-circular-pattern") { testCustomAxisCircularPattern(); std::cout<<"PASS: "<<checks<<" custom-axis-circular-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--asymmetric-chamfer") { testAsymmetricChamfer(); std::cout<<"PASS: "<<checks<<" asymmetric-chamfer assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--multi-edge-blends") { testMultiEdgeBlends(); std::cout<<"PASS: "<<checks<<" multi-edge-blend assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--oriented-extrusion") { testOrientedExtrusion(); std::cout<<"PASS: "<<checks<<" oriented-extrusion assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--custom-plane-extrusion") { testCustomPlaneExtrusion(); std::cout<<"PASS: "<<checks<<" custom-plane-extrusion assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--custom-axis-revolve") { testCustomAxisRevolve(); std::cout<<"PASS: "<<checks<<" custom-axis-revolve assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--tapered-helix") { testTaperedHelicalSweep(); std::cout<<"PASS: "<<checks<<" tapered-helix assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--form-features") { testFormFeatures(); std::cout<<"PASS: "<<checks<<" form-feature assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--vector-pattern") { testVectorLinearPattern(); std::cout<<"PASS: "<<checks<<" vector-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--vector-rectangular-pattern") { testVectorRectangularPattern(); std::cout<<"PASS: "<<checks<<" vector-rectangular-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--minimum-distance") { testMinimumDistanceMeasurement(); std::cout<<"PASS: "<<checks<<" minimum-distance assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--interference") { testInterferenceMeasurement(); std::cout<<"PASS: "<<checks<<" interference assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--body-diagnostics") { testBodyDiagnostics(); std::cout<<"PASS: "<<checks<<" body-diagnostics assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--density-mass-properties") { testDensityMassProperties(); std::cout<<"PASS: "<<checks<<" density-mass-properties assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--material-engineering") { testMaterialEngineeringRecord(); std::cout<<"PASS: "<<checks<<" material-engineering assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--body-materials") { testBodyMaterialAssignment(); std::cout<<"PASS: "<<checks<<" body-material assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--body-schedule-csv") { testBodyScheduleCsv(); std::cout<<"PASS: "<<checks<<" body-schedule-csv assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--bom-parts-standards") { testBomPartsStandards(); std::cout<<"PASS: "<<checks<<" BOM/parts/standards assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--occurrence-bom-metadata") { testOccurrenceBomMetadataInheritance(); std::cout<<"PASS: "<<checks<<" occurrence-BOM metadata assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-bom-group-keys") { testAssemblyBomGroupKeyBoundaries(); std::cout<<"PASS: "<<checks<<" assembly-bom-group-key assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-bom-full-scene") { testFullAssemblyBomBeyondSelectionCap(); std::cout<<"PASS: "<<checks<<" assembly-bom-full-scene assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-free-occurrence-yaw") { testFreeOccurrenceYawPlacement(); std::cout<<"PASS: "<<checks<<" assembly-free-occurrence-yaw assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--persistent-assemblies") { testPersistentAssemblyOccurrences(); std::cout<<"PASS: "<<checks<<" persistent-assembly assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--component-occurrence-patterns") { testComponentOccurrencePatterns(); std::cout<<"PASS: "<<checks<<" component-occurrence-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--step-assembly-import") { testStepAssemblyImport(); std::cout<<"PASS: "<<checks<<" STEP assembly-import assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--stl-mesh-import") { testStlMeshImport(); std::cout<<"PASS: "<<checks<<" STL mesh-import assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--iges-exchange") { testIgesExchange(); std::cout<<"PASS: "<<checks<<" IGES exchange assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--obj-mesh-import") { testObjMeshImport(); std::cout<<"PASS: "<<checks<<" OBJ mesh-import assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--parametric-foundation") { testParametricFoundation(); std::cout<<"PASS: "<<checks<<" parametric-foundation assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--parametric-links") { testParametricLinks(); std::cout<<"PASS: "<<checks<<" parametric-link assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--parametric-blind-hole") { testParametricBlindHolePattern(); testParametricBlindHole(); std::cout<<"PASS: "<<checks<<" parametric-blind-hole assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--parametric-feature-suppression") { testParametricFeatureSuppression(); std::cout<<"PASS: "<<checks<<" parametric-feature-suppression assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-joint-coordinate-guards") { testAssemblyJointCoordinateGuards(); std::cout<<"PASS: "<<checks<<" assembly-joint-coordinate-guard assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-joint-persistence-guards") { testAssemblyJointPersistenceGuards(); std::cout<<"PASS: "<<checks<<" assembly-joint-persistence-guard assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-sync-revolute-guard") { testAssemblySynchronizationRevoluteGuard(); std::cout<<"PASS: "<<checks<<" assembly-sync-revolute-guard assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-occurrence-pose-copy") { testAssemblyOccurrencePoseCopyGuard(); std::cout<<"PASS: "<<checks<<" assembly-occurrence-pose-copy assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--assembly-joints") { testMotionInterferenceScan(); testAssemblyJoints(); std::cout<<"PASS: "<<checks<<" assembly-joint assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--surface-body-persistence") { testSurfaceBodyPersistence(); std::cout<<"PASS: "<<checks<<" surface-body-persistence assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--sheet-metal-flat-pattern") { testSheetMetalFlatPattern(); std::cout<<"PASS: "<<checks<<" sheet-metal assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--folded-sheet-metal") { testFoldedSheetMetal(); std::cout<<"PASS: "<<checks<<" folded-sheet-metal assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--orthographic-drawing") { testDrawingSourceRevisionEdits(); testDrawingSheetIdentityUniqueness(); testDrawingExportPreflightDoesNotAcknowledgeFailedSnapshot(); testSelectedEdgeDrawingDimension(); testOrthographicDrawingExport(); std::cout<<"PASS: "<<checks<<" orthographic-drawing assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--planar-section") { testPlanarSectionMeasurement(); std::cout<<"PASS: "<<checks<<" planar-section assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--surface-summary") { testSurfaceTypeSummary(); std::cout<<"PASS: "<<checks<<" surface-summary assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--face-area") { testSelectedFaceArea(); std::cout<<"PASS: "<<checks<<" face-area assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--face-angle") { testPlanarFaceAngleMeasurement(); std::cout<<"PASS: "<<checks<<" face-angle assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--cylindrical-face") { testCylindricalFaceMeasurement(); std::cout<<"PASS: "<<checks<<" cylindrical-face assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--edge-length") { testSelectedEdgeLength(); std::cout<<"PASS: "<<checks<<" edge-length assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--vertex-coordinates") { testSelectedVertexCoordinates(); std::cout<<"PASS: "<<checks<<" vertex-coordinate assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--vertex-distance") { testVertexDistance(); std::cout<<"PASS: "<<checks<<" vertex-distance assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--circular-hole-pattern") { testCircularHolePattern(); std::cout<<"PASS: "<<checks<<" circular-hole-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--helical-sweep") { testHelicalSweep(); std::cout<<"PASS: "<<checks<<" helical-sweep assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--variable-pitch-helix") { testVariablePitchHelicalSweep(); std::cout<<"PASS: "<<checks<<" variable-pitch-helix assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--dovetail-groove") { testDovetailGroove(); std::cout<<"PASS: "<<checks<<" dovetail-groove assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--t-slot-groove") { testTSlotGroove(); std::cout<<"PASS: "<<checks<<" t-slot-groove assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--rounded-slot") { testRoundedSlot(); std::cout<<"PASS: "<<checks<<" rounded-slot assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--keyhole-slot") { testKeyholeSlot(); std::cout<<"PASS: "<<checks<<" keyhole-slot assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--rounded-slot-pattern") { testRoundedSlotPattern(); std::cout<<"PASS: "<<checks<<" rounded-slot-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--circular-countersink-pattern") { testCircularCountersinkPattern(); std::cout<<"PASS: "<<checks<<" circular-countersink-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--circular-counterbore-pattern") { testCircularCounterborePattern(); std::cout<<"PASS: "<<checks<<" circular-counterbore-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--linear-keyhole-slot-pattern") { testLinearKeyholeSlotPattern(); std::cout<<"PASS: "<<checks<<" linear-keyhole-slot-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--linear-counterbore-pattern") { testLinearCounterborePattern(); std::cout<<"PASS: "<<checks<<" linear-counterbore-pattern assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--linear-countersink-pattern") { testLinearCountersinkPattern(); std::cout<<"PASS: "<<checks<<" linear-countersink-pattern assertions\n"; return 0; }
        execute(); return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}

