#include "../src/core/RefoldGeometryAudit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static int checks=0;
static void check(bool valid,const char* reason){++checks;if(!valid)throw std::runtime_error(reason);}
int main(){try{
    using cad::detail::ContourPoint;
    using cad::refold::Audit;
    using cad::refold::Bend;
    using cad::refold::inspect;
    const std::vector<double> flanges{40,30};
    const std::vector<Bend> bend{{90,2}};
    // A rectangular straight strip has the same area as the unfolded strip;
    // injecting a bend without adding arc area must fail conservation.
    const std::vector<ContourPoint> ordinary{{0,0},{35,0},{70,0},{70,1},{35,1},{0,1}};
    Audit untouched;untouched.relativeAreaError=7;
    std::string why;
    check(!inspect(flanges,1,25,bend,ordinary,&untouched,&why),"flat rectangle falsely passed as folded profile");
    check(untouched.relativeAreaError==7,"rejected refold changed output");
    const long double PI=3.141592653589793238462643383279502884L;
    const double developed=70+(PI/2)*(2.0+0.5);
    const std::vector<ContourPoint> accurate{{0,0},{developed/2,0},{developed,0},{developed,1},{developed/2,1},{0,1}};
    check(inspect(flanges,1,25,bend,accurate,&untouched,&why),"refold conservation could not measure rectangle of equal area");
    check(std::abs(untouched.exactMidSurfaceDevelopedLengthMm-developed)<1e-11,"refold bend allowance incorrect");
    check(std::abs(untouched.predictedSolidVolumeMm3-25*developed)<1e-9,"refold volume incorrect");
    check(untouched.bendCount==1&&untouched.reversedBendCount==0,"bend metadata incorrect");
    const std::vector<Bend> returns{{90,2},{-120,3},{100,4}};
    const std::vector<double> four{20,25,30,35};
    long double developed4=110;
    for(const auto& b:returns)developed4+=(PI/180)*std::abs(b.signedDegrees)*(b.insideRadiusMm+0.6);
    const double len=static_cast<double>(developed4);
    const std::vector<ContourPoint> good{{0,0},{len/2,0},{len,0},{len,1.2},{len/2,1.2},{0,1.2}};
    check(inspect(four,1.2,40,returns,good,&untouched,&why),"three mixed-sign bends failed conservation");
    check(untouched.reversedBendCount==2,"reverse fold directions not tracked");
    // Tamper with the profile area while keeping a legitimate simple contour.
    const auto prior=untouched;
    const std::vector<ContourPoint> tooSmall{{0,0},{len/2,0},{len-4,0},{len-4,1.2},{len/2,1.2},{0,1.2}};
    check(!inspect(four,1.2,40,returns,tooSmall,&untouched,&why),"wrong refold geometry accepted");
    check(untouched.predictedSolidVolumeMm3==prior.predictedSolidVolumeMm3,"invalid refold changed prior report");
    const std::vector<ContourPoint> crossing{{0,0},{10,5},{0,5},{10,0}};
    check(!inspect(four,1.2,40,returns,crossing,&untouched,&why),"self-crossing polygon accepted");
    check(!inspect(four,1.2,40,{{190,2},{-120,3},{100,4}},good,&untouched,&why),"invalid angle accepted");
    check(!inspect(four,1.2,40,{{90,0},{-120,3},{100,4}},good,&untouched,&why),"zero radius accepted");
    check(!inspect(four,1.2,40,returns,good,nullptr,&why),"null report accepted");
    // Deterministic and repeatable without carrying mutable history.
    for(int i=0;i<25000;++i){
        Audit result;
        check(inspect(four,1.2,40,returns,good,&result,&why),"repeatable geometry conservation");
        check(result.predictedSolidVolumeMm3==prior.predictedSolidVolumeMm3,"nondeterministic refold area");
    }
    std::cout<<checks<<" independent refold geometry checks passed\n";
    return 0;
}catch(const std::exception& exc){std::cerr<<exc.what()<<"\n";return 1;}}
