#include "core/SceneDocument.h"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>
#include <iostream>
namespace {
int checks=0;
void must(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
cad::SketchModel sketch(double x){
    cad::SketchModel s;
    s.points={{"a",0,0},{"b",x,0},{"c",x,10},{"d",0,10}};
    s.segments={{"e1","a","b"},{"e2","b","c"},{"e3","c","d"},{"e4","d","a"}};
    return s;
}
QStringList boundary(){return {"a","b","c","d"};}
double volume(const cad::SceneDocument& d,const QString& id){
    GProp_GProps p;BRepGProp::VolumeProperties(d.find(id)->shape,p);return p.Mass();
}
void run(){
    QString error;cad::SceneDocument d;
    must(d.addConstrainedPolygonExtrusion(sketch(10),boundary(),10,&error),"sketch source");
    const QString source=d.parts().last().id;
    must(d.addPrimitive(cad::Primitive::Box,{12,12,10},&error),"target");
    const QString target=d.parts().last().id;
    must(d.setInPlaceSketchFeature(target,source,cad::BooleanOp::Fuse,5,8,&error),"external boss");
    must(std::abs(volume(d,target)-1740)<0.001,"boss volume");
    cad::ShapeFeatureNode translate;
    translate.id="mixed-translate";translate.type="Translate";
    translate.parameters={{"x",20},{"y",0},{"z",0}};
    must(d.setShapeFeatureTree(target,{translate},&error),"general feature follows sketch operation");
    must(d.find(target)->hasShapeFeatureTree&&d.find(target)->hasInPlaceSketchFeature,"mixed metadata");
    must(std::abs(volume(d,target)-1740)<0.001,"post-translate volume");
    must(d.updateConstrainedPolygonExtrusion(source,sketch(8),boundary(),10,&error),"upstream sketch edit");
    must(std::abs(volume(d,target)-1680)<0.001,"mixed history did not rebuild");
    must(d.shapeFeatureTreeCurrent(target),"general tree became stale");
    must(d.inPlaceSketchFeatureCurrent(target),"source link became stale");
    // Editing upstream modifier parameters is allowed even after the general
    // tree has been appended. Every downstream operation must replay.
    must(d.setInPlaceSketchFeature(target,source,cad::BooleanOp::Fuse,4,8,&error),
         "edit external sketch stage after general feature tree");
    must(std::abs(volume(d,target)-1600)<0.001,"edited mixed-history volume");
    must(d.setInPlaceSketchFeature(target,source,cad::BooleanOp::Fuse,5,8,&error),
         "restore upstream sketch depth");
    must(std::abs(volume(d,target)-1680)<0.001,"restored mixed-history volume");
    const QString stable=d.find(target)->geometryToken;
    must(!d.setInPlaceSketchFeature(target,source,cad::BooleanOp::Fuse,5,80,&error),
         "detached stage unexpectedly accepted");
    must(d.find(target)->geometryToken==stable,"failed mixed edit changed body");
    QTemporaryDir tmp;must(tmp.isValid(),"temp dir");
    const auto path=tmp.path()+"/mixed.cadscene";
    must(d.saveAs(path,&error),"save combined history");
    cad::SceneDocument reopened;
    must(reopened.load(path,&error),"load combined history");
    must(std::abs(volume(reopened,target)-1680)<0.001,"saved mixed volume");
    must(reopened.updateConstrainedPolygonExtrusion(source,sketch(7),boundary(),10,&error),"edit after reopen");
    must(std::abs(volume(reopened,target)-1650)<0.001,"reopened mixed history regeneration");
    must(reopened.removeInPlaceSketchFeature(target,&error),"remove source modifier but keep downstream history");
    must(std::abs(volume(reopened,target)-1440)<0.001,"remove sketch modifier retained base geometry");
    must(reopened.find(target)->hasShapeFeatureTree,"general history lost after remove");
    must(reopened.undo(),"undo mixed removal");
    must(std::abs(volume(reopened,target)-1650)<0.001,"undo mixed removal volume");
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);
try{run();std::cout<<checks<<" mixed-history checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
