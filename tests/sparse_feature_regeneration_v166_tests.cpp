#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
void near(double actual,double expected,double delta,const char* what){
    check(std::abs(actual-expected)<delta,what);
}
cad::ShapeFeatureNode translate(const char* id,double x,double y,const QStringList& inputs){
    cad::ShapeFeatureNode node;
    node.id=QString::fromLatin1(id);node.type="Translate";
    node.parameters={{"x",x},{"y",y},{"z",0}};
    node.inputs=inputs;
    return node;
}
double volume(const TopoDS_Shape& shape){
    GProp_GProps props;BRepGProp::VolumeProperties(shape,props);return props.Mass();
}
void testRebuildingAndPersistence(){
    cad::SceneDocument doc;QString error;
    check(doc.addPrimitive(cad::Primitive::Box,{10,10,10},&error),"Fixture primitive failed");
    const QString partId=doc.parts().last().id;
    auto left=translate("left",4,0,{"@base"});
    auto right=translate("right",0,4,{"@base"});
    auto leftTip=translate("leftTip",1,0,{"left"});
    auto rightTip=translate("rightTip",0,1,{"right"});
    cad::ShapeFeatureNode unionNode;
    unionNode.id="fusion";unionNode.type="Fuse branches";
    unionNode.inputs={"leftTip","rightTip"};
    QVector<cad::ShapeFeatureNode> tree={left,right,leftTip,rightTip,unionNode};
    check(doc.setShapeFeatureTree(partId,tree,&error),"Initial branching feature tree failed");
    check(doc.find(partId)->shapeFeatureCachedOutputs.size()==5,"Initial outputs not cached");
    const TopoDS_Shape cachedRight=doc.find(partId)->shapeFeatureCachedOutputs[1];
    const TopoDS_Shape cachedRightTip=doc.find(partId)->shapeFeatureCachedOutputs[3];
    const TopoDS_Shape oldBody=doc.find(partId)->shape;
    tree[0].parameters["x"]=6;
    QVector<bool> available={true,true,true,true,true};
    QVector<int> previous;
    check(cad::planShapeFeatureReuse(doc.find(partId)->shapeFeatureNodes,tree,available,&previous,&error),
          "Could not plan branch reuse");
    check(previous==QVector<int>({-1,1,-1,3,-1}),"Unexpected sparse reuse planner result");
    check(doc.setShapeFeatureTree(partId,tree,&error),"Editing left branch failed");
    check(doc.find(partId)->shapeFeatureCachedOutputs[1].IsSame(cachedRight),"Independent right branch was rebuilt");
    check(doc.find(partId)->shapeFeatureCachedOutputs[3].IsSame(cachedRightTip),"Independent right child was rebuilt");
    check(!doc.find(partId)->shape.IsSame(oldBody),"Dependent fusion output was not rebuilt");
    const TopoDS_Shape committed=doc.find(partId)->shape;
    TopoDS_Shape fresh;check(cad::rebuildShapeFeatureTree(doc.find(partId)->shapeFeatureBase,false,tree,&fresh,&error),
        "Fresh deterministic rebuild failed");
    near(volume(committed),volume(fresh),1e-4,"Sparse rebuild differs from full rebuild");
    // Reorder independent nodes while preserving every explicit predecessor.
    tree.swapItemsAt(0,1);
    tree.swapItemsAt(2,3);
    check(doc.setShapeFeatureTree(partId,tree,&error),"Independent branch reordering failed");
    check(doc.find(partId)->shapeFeatureCachedOutputs[0].IsSame(cachedRight),"Moved branch was needlessly rebuilt");
    check(doc.find(partId)->shapeFeatureCachedOutputs[2].IsSame(cachedRightTip),"Moved child lost cached output");
    const auto acceptedToken=doc.find(partId)->geometryToken;
    const auto acceptedVolume=volume(doc.find(partId)->shape);
    // Failure after sparse reuse must not partially commit output/cache/undo.
    tree[1].parameters["x"]=1000001.;
    check(!doc.setShapeFeatureTree(partId,tree,&error),"Out of bounds feature was accepted");
    check(doc.find(partId)->geometryToken==acceptedToken,"Rejected feature changed document token");
    near(volume(doc.find(partId)->shape),acceptedVolume,1e-4,"Rejected feature changed volume");
    check(doc.find(partId)->shapeFeatureCachedOutputs[0].IsSame(cachedRight),"Rejected change mutated cached independent branch");
    check(doc.undo(),"Undo failed");
    check(doc.redo(),"Redo failed");
    near(volume(doc.find(partId)->shape),acceptedVolume,1e-4,"Redo changed accepted geometry");
    QTemporaryDir temporary;
    check(temporary.isValid(),"Cannot make temp directory");
    const QString file=temporary.path()+"/sparse.cadscene";
    check(doc.saveAs(file,&error),"Save failed");
    cad::SceneDocument loaded;
    check(loaded.load(file,&error),"Reload failed");
    check(loaded.find(partId)->shapeFeatureCachedOutputs.size()==5,"Load did not reconstruct caches");
    auto following=loaded.find(partId)->shapeFeatureNodes;
    const TopoDS_Shape rightCachedAfterReload=loaded.find(partId)->shapeFeatureCachedOutputs[0];
    following[1].parameters["x"]=8;
    check(loaded.setShapeFeatureTree(partId,following,&error),"Edit after reload failed");
    check(loaded.find(partId)->shapeFeatureCachedOutputs[0].IsSame(rightCachedAfterReload),"Reloaded independent cache not reused");
    check(doc.find(partId)->shapeFeatureNodes[0].id=="right","Original document was mutated by reload");
}
}
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);
    try{testRebuildingAndPersistence();std::cout<<"PASS: "<<checks<<" native sparse feature geometry checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL check "<<checks<<": "<<e.what()<<'\n';return 1;}
}
