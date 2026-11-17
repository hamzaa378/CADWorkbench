#pragma once
// Native regression cases included in existing CTest modes to preserve the
// original 130-case Windows audit inventory.
#include "core/SceneDocument.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <QCoreApplication>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace cad::sprint7_test {
int checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct Fixture {
    cad::SceneDocument doc;
    QString root,tool,dependent;
    void create(bool intersection=false){
        QString issue;
        require(doc.addPrimitive(cad::Primitive::Box,{40,30,15},&issue),"create root box");
        root=doc.parts().last().id;
        require(doc.addPrimitive(cad::Primitive::Box,{10,10,8},&issue),"create cutter dependency box");
        tool=doc.parts().last().id;
        require(doc.translate(tool,10,0,0,&issue),"position dependency body");
        require(doc.addLinkedBooleanBody(intersection?cad::BooleanOp::Common:cad::BooleanOp::Fuse,
                                         root,tool,&issue),"create linked Boolean descendant");
        dependent=doc.parts().last().id;
        require(doc.linkedBooleanCurrent(dependent),"initial dependent revision current");
    }
    TopoDS_Shape cut() const {
        Bnd_Box bounds;BRepBndLib::Add(doc.find(root)->shape,bounds);
        double xmin,ymin,zmin,xmax,ymax,zmax;
        bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        const double radius=std::min(xmax-xmin,ymax-ymin)/10.0;
        const auto toolShape=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt((xmin+xmax)*0.5,(ymin+ymax)*0.5,zmin-2),gp_Dir(0,0,1)),
            radius,zmax-zmin+4).Shape();
        BRepAlgoAPI_Cut op(doc.find(root)->shape,toolShape);op.Build();
        require(op.IsDone()&&!op.Shape().IsNull(),"make actual solid cut preview");
        return op.Shape();
    }
    void afterSuccessfulCommit(const QString& beforeRoot,const QString& beforeChild){
        require(doc.find(root)->geometryToken!=beforeRoot,"source revision not changed");
        require(doc.find(dependent)->geometryToken!=beforeChild,"linked descendant not regenerated");
        require(doc.linkedBooleanCurrent(dependent),"linked descendant not current");
        require(doc.undo(),"one-step machining undo unavailable");
        require(doc.find(root)->geometryToken==beforeRoot,"undo did not restore source revision");
        require(doc.find(dependent)->geometryToken==beforeChild,"undo did not restore dependent revision");
        require(doc.redo(),"one-step machining redo unavailable");
        require(doc.linkedBooleanCurrent(dependent),"redo did not restore live descendant");
    }
};
void commitMachiningVariants(){
    for(int kind=0;kind<5;++kind){
        Fixture f;f.create();QString issue;
        const QString sourceRev=f.doc.find(f.root)->geometryToken;
        const QString linkedRev=f.doc.find(f.dependent)->geometryToken;
        const TopoDS_Shape preview=f.cut();
        bool success=false;
        switch(kind){
        case 0:{cad::DovetailGrooveCommand cmd;cmd.partId=f.root;cmd.geometryToken=sourceRev;
                success=f.doc.commitDovetailGroove(cmd,preview,&issue);break;}
        case 1:{cad::TSlotGrooveCommand cmd;cmd.partId=f.root;cmd.geometryToken=sourceRev;
                success=f.doc.commitTSlotGroove(cmd,preview,&issue);break;}
        case 2:{cad::RoundedSlotCommand cmd;cmd.partId=f.root;cmd.geometryToken=sourceRev;
                success=f.doc.commitRoundedSlot(cmd,preview,&issue);break;}
        case 3:{cad::KeyholeSlotCommand cmd;cmd.partId=f.root;cmd.geometryToken=sourceRev;
                success=f.doc.commitKeyholeSlot(cmd,preview,&issue);break;}
        default:{cad::RoundedSlotPatternCommand cmd;cmd.partId=f.root;cmd.geometryToken=sourceRev;
                success=f.doc.commitRoundedSlotPattern(cmd,preview,&issue);break;}
        }
        if(!success)throw std::runtime_error("machining transaction failed: "+issue.toStdString());
        f.afterSuccessfulCommit(sourceRev,linkedRev);
    }
}
void rejectionPreservesScene(){
    Fixture f;f.create(true);QString issue;
    const QString rev=f.doc.find(f.root)->geometryToken;
    const QString childRev=f.doc.find(f.dependent)->geometryToken;
    cad::DovetailGrooveCommand cmd;cmd.partId=f.root;cmd.geometryToken=rev;
    require(!f.doc.commitDovetailGroove(cmd,TopoDS_Shape{},&issue),"null preview accepted");
    require(f.doc.find(f.root)->geometryToken==rev,"null preview changed source");
    require(f.doc.find(f.dependent)->geometryToken==childRev,"null preview changed descendant");
    const auto preview=f.cut();
    cmd.geometryToken="unknown-old-revision";
    require(!f.doc.commitDovetailGroove(cmd,preview,&issue),"stale preview accepted");
    require(f.doc.find(f.root)->geometryToken==rev,"stale preview changed source");
    cmd.geometryToken=rev;
    const TopoDS_Shape adding=BRepPrimAPI_MakeBox(gp_Pnt(-40,-30,-10),100,90,60).Shape();
    require(!f.doc.commitDovetailGroove(cmd,adding,&issue),"additive preview accepted as cutter");
    require(f.doc.find(f.root)->geometryToken==rev,"invalid material-adding preview changed body");
    cmd.partId=f.dependent;
    cmd.geometryToken=f.doc.find(f.dependent)->geometryToken;
    require(!f.doc.commitDovetailGroove(cmd,preview,&issue),"linked derived body directly edited");
    require(f.doc.find(f.dependent)->geometryToken==childRev,"derived edit changed body");
    cmd.partId=f.root;cmd.geometryToken=rev;
    const TopoDS_Shape farAway=BRepPrimAPI_MakeBox(gp_Pnt(1000,1000,1000),12,12,8).Shape();
    require(!f.doc.commitDovetailGroove(cmd,farAway,&issue),
            "rebuild failure accepted disconnected common dependency");
    require(f.doc.find(f.root)->geometryToken==rev,"failed regeneration committed source");
    require(f.doc.find(f.dependent)->geometryToken==childRev,"failed regeneration committed child");
    require(f.doc.linkedBooleanCurrent(f.dependent),"failed regeneration left stale linked state");
}
void patternsUpdateDescendants(){
    Fixture f;f.create();QString issue;
    Bnd_Box bounds;BRepBndLib::Add(f.doc.find(f.root)->shape,bounds);
    double xmin,ymin,zmin,xmax,ymax,zmax;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double cx=(xmin+xmax)*0.5,cy=(ymin+ymax)*0.5;
    const QString first=f.doc.find(f.root)->geometryToken;
    const QString child=f.doc.find(f.dependent)->geometryToken;
    if(!f.doc.holePattern(f.root,cx,cy,zmax,2.0,10.0,true,2,7.0,1,0.0,&issue))
        throw std::runtime_error("hole grid rejected: "+issue.toStdString());
    f.afterSuccessfulCommit(first,child);
}
} // namespace cad::sprint7_test
