#include "core/JointMotionInterpolation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {int count=0;void require(bool yes,const char* message){++count;if(!yes)throw std::runtime_error(message);}}
int main(){
 try{
 using namespace cad::joint_motion;
 std::vector<std::pair<int,double>> output{{999,123}};
 std::string error="old";
 const std::vector<Drive> drives={{1,-10,10,-20,20},{2,20,-20,-25,25}};
 require(interpolate(drives,0,Progression::Linear,&output,&error),"linear start");
 require(output.size()==2&&output[0].first==1&&output[0].second==-10&&output[1].second==20,"wrong initial");
 require(interpolate(drives,1,Progression::Smoothstep,&output,&error),"smooth end");
 require(output[0].second==10&&output[1].second==-20,"wrong end");
 require(interpolate(drives,.25,Progression::Linear,&output,&error),"linear quarter");
 require(std::abs(output[0].second+5)<1e-13,"linear fraction incorrect");
 require(interpolate(drives,.25,Progression::Smoothstep,&output,&error),"smooth quarter");
 require(std::abs(output[0].second+6.875)<1e-13,"smoothstep fraction incorrect");
 require(interpolate(drives,.5,Progression::Smoothstep,&output,&error),"smooth midpoint");
 require(output[0].second==0&&output[1].second==0,"smooth midpoint incorrect");
 for(int n=0;n<=100;++n){
   const double f=static_cast<double>(n)/100.0;
   require(interpolate(drives,f,Progression::Smoothstep,&output,&error),"smooth interpolation failed");
   require(output[0].second>=-20&&output[0].second<=20&&
           output[1].second>=-25&&output[1].second<=25,"interpolation escaped limits");
 }
 const auto saved=output;
 require(!interpolate(drives,-.1,Progression::Linear,&output,&error),"negative fraction accepted");
 require(output==saved,"negative fraction mutated output");
 require(!interpolate(drives,1.01,Progression::Smoothstep,&output,&error),"fraction above one accepted");
 require(output==saved,"fraction above one mutated output");
 require(!interpolate(drives,std::numeric_limits<double>::quiet_NaN(),Progression::Linear,&output,&error),"nan fraction accepted");
 require(output==saved,"nan mutated output");
 require(!interpolate({{1,0,3,0,2}},.5,Progression::Linear,&output,&error),"outside travel limits accepted");
 require(output==saved,"outside limits mutated output");
 require(!interpolate({{1,0,2,0,3},{1,1,2,0,3}},.5,Progression::Linear,&output,&error),"duplicate drive accepted");
 require(output==saved,"duplicate mutated output");
 require(!interpolate({{2,0,1,2,3}},.5,Progression::Linear,&output,&error),"invalid initial accepted");
 require(output==saved,"invalid initial mutated output");
 require(!interpolate(drives,.5,static_cast<Progression>(7),&output,&error),"invalid mode accepted");
 require(output==saved,"invalid mode mutated output");
 require(!interpolate({},.5,Progression::Linear,&output,&error),"empty drives accepted");
 require(output==saved,"empty input mutated output");
 require(!interpolate(drives,.5,Progression::Linear,nullptr,&error),"null output accepted");
 require(interpolate({{1,0,4,0,4}},1,Progression::Smoothstep,&output,&error)&&output[0].second==4,"legacy normalized bounds");
 std::cout<<count<<" motion interpolation assertions passed\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
