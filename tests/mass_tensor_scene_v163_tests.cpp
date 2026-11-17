#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void near(double value,double expected,const char* why,double tolerance=1e-6){
    require(std::abs(value-expected)<=tolerance*std::max(1.0,std::abs(expected)),why);
}
}
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);
    try{
        QString error;
        cad::SceneDocument doc;
        require(doc.addPrimitive(cad::Primitive::Box,{10,20,30},&error),"Failed to make box fixture");
        const QString id=doc.parts().first().id;
        cad::tensor::Report report;
        require(doc.measureMassTensor(id,7850,&report,&error),"Mass tensor query failed");
        const double mass=10.0*20.0*30.0*7850.0/1e9;
        near(report.massKg,mass,"Box mass wrong");
        near(report.centerMm[0],5.0,"Center X wrong");
        near(report.centerMm[1],10.0,"Center Y wrong");
        near(report.centerMm[2],15.0,"Center Z wrong");
        near(report.inertiaKgMm2[0],mass*(20.0*20.0+30.0*30.0)/12.0,"Ixx wrong");
        near(report.inertiaKgMm2[4],mass*(10.0*10.0+30.0*30.0)/12.0,"Iyy wrong");
        near(report.inertiaKgMm2[8],mass*(10.0*10.0+20.0*20.0)/12.0,"Izz wrong");
        require(report.principalAxesUnique,"Box principal axes should be unique");
        const double originalLeast=report.principalMomentsKgMm2[0];
        const bool hasHistoryBefore=doc.canUndo();
        require(doc.measureMassTensor(id,2700,&report,&error),"Aluminum measurement failed");
        near(report.principalMomentsKgMm2[0]/originalLeast,2700.0/7850.0,"Density scaling wrong");
        require(doc.canUndo()==hasHistoryBefore,"Read-only measurement changed undo history");
        const cad::ShapeFeatureNode move{QStringLiteral("move"),QStringLiteral("Translate"),
                                         {{QStringLiteral("x"),100.0},{QStringLiteral("y"),-50.0},{QStringLiteral("z"),25.0}},false};
        require(doc.setShapeFeatureTree(id,{move},&error),"Move feature failed");
        require(doc.measureMassTensor(id,7850,&report,&error),"Moved body measurement failed");
        near(report.centerMm[0],105.0,"Translated center X wrong");
        near(report.centerMm[1],-40.0,"Translated center Y wrong");
        near(report.centerMm[2],40.0,"Translated center Z wrong");
        near(report.principalMomentsKgMm2[0],originalLeast,"Rigid translation changed centroidal moment");
        require(doc.undo(),"Undo move failed");
        require(doc.measureMassTensor(id,7850,&report,&error),"Undo measurement failed");
        near(report.centerMm[0],5.0,"Undo did not restore original center");
        require(doc.redo(),"Redo move failed");
        require(doc.measureMassTensor(id,7850,&report,&error),"Redo measurement failed");
        near(report.centerMm[0],105.0,"Redo did not restore translated center");
        QTemporaryDir scratch;
        require(scratch.isValid(),"Temporary directory failed");
        const QString filename=scratch.filePath("inertia.cadscene");
        require(doc.saveAs(filename,&error),"Could not save moved body");
        cad::SceneDocument loaded;
        require(loaded.load(filename,&error),"Could not reload moved body");
        require(loaded.measureMassTensor(id,7850,&report,&error),"Loaded body measurement failed");
        near(report.centerMm[0],105.0,"Reload altered center of mass");
        near(report.principalMomentsKgMm2[0],originalLeast,"Reload altered inertia");
        const auto previous=report;
        require(!loaded.measureMassTensor(id,0,&report,&error),"Zero density accepted");
        near(report.massKg,previous.massKg,"Invalid measurement changed previous report");
        require(!loaded.measureMassTensor(id,7850,nullptr,&error),"Null report accepted");
        require(!loaded.measureMassTensor(QStringLiteral("not-present"),7850,&report,&error),"Missing body accepted");
        cad::SceneDocument assembly;
        require(assembly.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Assembly first body failed");
        const QString first=assembly.parts().last().id;
        require(assembly.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Assembly second body failed");
        const QString second=assembly.parts().last().id;
        require(!assembly.measureAssemblyMassTensor({first,second},&report,&error),
                "Assembly should require assigned material density");
        require(assembly.setMaterial(QStringList{first,second},QStringLiteral("Test steel"),7850,&error),
                "Assembly material assignment failed");
        const cad::ShapeFeatureNode shift{QStringLiteral("shift"),QStringLiteral("Translate"),
                                          {{QStringLiteral("x"),20.0},{QStringLiteral("y"),0.0},
                                           {QStringLiteral("z"),0.0}},false};
        require(assembly.setShapeFeatureTree(second,{shift},&error),"Assembly translation failed");
        require(assembly.measureAssemblyMassTensor({first,second},&report,&error),"Assembly tensor failed");
        near(report.massKg,2*1000.0*7850.0/1e9,"Assembly mass wrong");
        near(report.centerMm[0],15.0,"Assembly center wrong");
        near(report.centerMm[1],5.0,"Assembly center Y wrong");
        const double singleMass=1000.0*7850.0/1e9;
        near(report.inertiaKgMm2[0],2*singleMass*100.0/6.0,"Assembly Ixx wrong");
        near(report.inertiaKgMm2[4],2*(singleMass*100.0/6.0+singleMass*100.0),
             "Assembly parallel-axis theorem wrong");
        require(!assembly.measureAssemblyMassTensor({first,first},&report,&error),
                "Duplicate assembly body accepted");
        require(!assembly.measureAssemblyMassTensor({},&report,&error),"Empty assembly accepted");
        require(!assembly.measureAssemblyMassTensor({first,QStringLiteral("missing")},&report,&error),
                "Missing assembly body accepted");
        require(!assembly.measureAssemblyMassTensor({first},nullptr,&error),"Null assembly result accepted");
        std::cout<<"PASS: "<<checks<<" native CAD tensor integration checks\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
