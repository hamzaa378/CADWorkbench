#include "core/SceneDocument.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void must(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
std::vector<cad::mates::Constraint> rigid(double x,double y,double z){
    using namespace cad::mates;
    std::vector<Constraint> c;
    for(Vec3 p:{Vec3{0,0,0},Vec3{2,0,0},Vec3{0,2,0}})
        c.push_back({Kind::Point,{p.x+x,p.y+y,p.z+z},p,{},{}});
    return c;
}
void run(){
    cad::SceneDocument scene;QString error;QStringList bodies;
    for(int i=0;i<4;++i){
        must(scene.addPrimitive(cad::Primitive::Box,{5,5,5},&error),"box");
        const QString id=scene.parts().last().id;
        must(scene.makeComponent(id,QStringLiteral("Node %1").arg(i),&error),"component");
        bodies<<id;
    }
    using E=cad::SceneDocument::AssemblyNetworkMate;
    QVector<E> loop={
        {bodies[0],bodies[1],rigid(10,0,0)},
        {bodies[1],bodies[2],rigid(0,20,0)},
        {bodies[2],bodies[3],rigid(-10,0,0)},
        {bodies[3],bodies[0],rigid(0,-20,0)}};
    must(scene.solveAssemblyMateNetwork(bodies[0],loop,&error),"closed loop document solve");
    must(std::abs(scene.find(bodies[1])->placementX-10)<1e-4,"node 1 pose");
    must(std::abs(scene.find(bodies[2])->placementY-20)<1e-4,"node 2 pose");
    must(std::abs(scene.find(bodies[3])->placementX+0)<1e-4,"node 3 pose");
    QTemporaryDir tmp;must(tmp.isValid(),"temp file");
    const QString file=tmp.path()+"/closed_loop.cadscene";
    must(scene.saveAs(file,&error),"save network placements");
    cad::SceneDocument reopened;must(reopened.load(file,&error),"reopen network placements");
    must(std::abs(reopened.find(bodies[2])->placementY-20)<1e-4,"network placement persisted");
    auto conflict=loop;conflict[3].constraints=rigid(0,-19,0);
    must(!scene.solveAssemblyMateNetwork(bodies[0],conflict,&error),"reject conflicting loop");
    must(std::abs(scene.find(bodies[2])->placementY-20)<1e-4,"rollback on conflict");
    must(scene.undo(),"single undo for full network");
    must(std::abs(scene.find(bodies[2])->placementY)<1e-4,"undone every component");
    must(scene.redo(),"redo full network");
    must(std::abs(scene.find(bodies[2])->placementY-20)<1e-4,"redone every component");
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{run();std::cout<<checks<<" scene checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
