#include "core/AssemblyConstraintMobility.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::mates;
static void expect(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
int main(){
try{
  int checks=0;std::string error;MobilityReport report;
  const Vec3 O{0,0,0}, X{1,0,0},Y{0,1,0},Z{0,0,1};
  auto check=[&](const std::vector<Constraint>& cs,int dof,int rank){
    expect(analyzeConstraintMobility(2,0,{{0,1,cs}},&report,&error),error.c_str());++checks;
    expect(report.remainingDegreesOfFreedom==dof,"incorrect mobility DOF");++checks;
    expect(report.independentConstraintRows==rank,"incorrect mobility rank");++checks;
    expect(report.disconnectedOccurrences.empty(),"incorrect disconnected report");++checks;
  };
  check({{Kind::Point,O,O,O,O}},3,3);
  check({{Kind::CoaxialAxis,O,O,Z,Z}},2,4);
  check({{Kind::CoincidentPlane,O,O,Z,Z}},3,3);
  check({{Kind::CoaxialAxis,O,O,Z,Z},
         {Kind::CoincidentPlane,O,O,Z,Z}},1,5);
  check({{Kind::Point,O,O,O,O},{Kind::Point,{0,0,10},{0,0,10},O,O}},1,5);
  check({{Kind::Point,O,O,O,O},{Kind::CoaxialAxis,O,O,X,X},
         {Kind::CoaxialAxis,O,O,Y,Y}},0,6);
  check({{Kind::Point,O,O,O,O},{Kind::Point,{1,0,0},{1,0,0},O,O},
         {Kind::Point,{0,2,0},{0,2,0},O,O}},0,6);
  expect(analyzeConstraintMobility(2,0,{},&report,&error),"empty unconnected network");++checks;
  expect(report.remainingDegreesOfFreedom==6,"empty network six DOF");++checks;
  expect(report.disconnectedOccurrences==std::vector<std::size_t>{1},"disconnected missing");++checks;
  std::vector<NetworkConnection> chain{{0,1,{{Kind::Point,O,O,O,O},
      {Kind::CoaxialAxis,O,O,X,X},{Kind::CoaxialAxis,O,O,Y,Y}}},
      {1,2,{{Kind::CoaxialAxis,O,O,Z,Z}}}};
  expect(analyzeConstraintMobility(3,0,chain,&report,&error),"three node network");++checks;
  expect(report.remainingDegreesOfFreedom==2,"partial hinge two DOF");++checks;
  chain.push_back(chain.back());
  expect(analyzeConstraintMobility(3,0,chain,&report,&error),"repeat constraint");++checks;
  expect(report.whollyRedundantConnections==std::vector<std::size_t>{3},"redundant connection");++checks;
  MobilityReport saved=report;
  chain[1].constraints[0].childDirection={0,0,-1};
  expect(!analyzeConstraintMobility(3,0,chain,&report,&error),"inconsistent axis should fail");++checks;
  expect(report.remainingDegreesOfFreedom==saved.remainingDegreesOfFreedom,"failure changed output");++checks;
  chain[1].constraints[0].childDirection=Z;
  chain[1].constraints[0].childPoint={0,1,0};
  expect(!analyzeConstraintMobility(3,0,chain,&report,&error),"non-coaxial should fail");++checks;
  std::vector<Constraint> offset{{Kind::CoincidentPlane,O,{0,0,5},Z,Z,5}};
  check(offset,3,3);
  offset[0].offsetMm=6;
  expect(!analyzeConstraintMobility(2,0,{{0,1,offset}},&report,&error),"plane separation mismatch");++checks;
  std::vector<Constraint> far{{Kind::Point,{100000,200000,300000},{100000,200000,300000},O,O},
      {Kind::CoaxialAxis,{100000,200000,300000},{100000,200000,300000},X,X},
      {Kind::CoaxialAxis,{100000,200000,300000},{100000,200000,300000},Y,Y}};
  check(far,0,6);
  expect(!analyzeConstraintMobility(33,0,{},&report,&error),"large node count rejection");++checks;
  std::cout<<"assembly mobility: "<<checks<<" checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}
}
