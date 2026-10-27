#include "core/SceneDocument.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

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
    require(stepTarget.parts()[0].name=="Sphere"&&stepTarget.parts()[1].name.startsWith("assembly")&&stepTarget.parts()[2].name.startsWith("assembly"),"STEP import did not preserve the existing body or assign imported names");
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
} // namespace
int main(int argc,char** argv) {
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
        if(argc==2&&std::string(argv[1])=="--body-materials") { testBodyMaterialAssignment(); std::cout<<"PASS: "<<checks<<" body-material assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--body-schedule-csv") { testBodyScheduleCsv(); std::cout<<"PASS: "<<checks<<" body-schedule-csv assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--planar-section") { testPlanarSectionMeasurement(); std::cout<<"PASS: "<<checks<<" planar-section assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--surface-summary") { testSurfaceTypeSummary(); std::cout<<"PASS: "<<checks<<" surface-summary assertions\n"; return 0; }
        if(argc==2&&std::string(argv[1])=="--face-area") { testSelectedFaceArea(); std::cout<<"PASS: "<<checks<<" face-area assertions\n"; return 0; }
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

