#include "core/PolygonFeatureProfile.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using cad::polygon::Point;
using cad::polygon::validate;
using cad::polygon::validateLoft;
namespace {
int checks=0;
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
}
int main(){
    try{
        const std::vector<Point> box={{1,1},{5,1},{5,4},{1,4}};
        check(validate(box,false),"Valid rectangle rejected");
        check(validate(box,true),"Valid radial rectangle rejected");
        check(std::abs(cad::polygon::signedArea(box)-12.)<1e-12,"Area incorrect");
        auto backwards=box;std::reverse(backwards.begin(),backwards.end());
        check(validate(backwards,false),"Clockwise valid polygon rejected");
        check(cad::polygon::signedArea(backwards)<0,"Reversed area sign wrong");
        check(!validate({{0,0},{1,1}},false),"2 points accepted");
        check(!validate({{0,0},{1,0},{2,0}},false),"Collinear points accepted");
        check(!validate({{0,0},{3,3},{0,3},{3,0}},false),"Bowtie accepted");
        check(!validate({{0,0},{3,0},{3,0},{0,3}},false),"Duplicate accepted");
        check(!validate({{0,0},{3,0},{3,2},{2,0},{0,2}},false),"Overlapping edge accepted");
        check(!validate({{0,0},{1,0},{1,1},{0,1}},true),"Radial axis contact accepted");
        auto nan=box;nan[1].x=std::numeric_limits<double>::quiet_NaN();
        check(!validate(nan,false),"NaN accepted");
        nan=box;nan[2].y=std::numeric_limits<double>::infinity();
        check(!validate(nan,false),"Inf accepted");
        nan=box;nan[0].x=-1e6;
        check(!validate(nan,false),"Extreme coordinate accepted");
        check(validateLoft(box,box,9.),"Equal-section loft rejected");
        auto larger=box;for(auto& p:larger){p.x*=1.5;p.y*=1.5;}
        check(validateLoft(box,larger,12.),"Tapered loft rejected");
        check(!validateLoft(box,larger,-1.),"Negative loft height accepted");
        check(!validateLoft(box,larger,0.),"Zero loft height accepted");
        check(!validateLoft(box,backwards,10.),"Inverted winding accepted");
        check(!validateLoft(box,{{1,1},{5,1},{3,4}},10.),"Mismatched vertices accepted");
        for(int k=3;k<=24;++k){
            std::vector<Point> regular;
            for(int i=0;i<k;++i){double theta=2*3.14159265358979323846*i/k;
                regular.push_back({20+10*std::cos(theta),10*std::sin(theta)});}
            check(validate(regular,false),"Regular polygon rejected");
            check(validate(regular,true),"Regular radial polygon rejected");
            check(validateLoft(regular,regular,1.+k),"Regular loft rejected");
        }
        std::vector<Point> many;
        for(int i=0;i<25;++i){double theta=2*3.14159265358979323846*i/25;
            many.push_back({20+10*std::cos(theta),10*std::sin(theta)});}
        check(!validate(many,false),"Too many points accepted");
        std::string issue;
        check(!validate({{0,0},{0,1}},false,&issue)&&!issue.empty(),"Error message missing");
        std::cout<<"PASS: "<<checks<<" polygon profile validation checks\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<" (check "<<checks<<")\n";return 1;}
}
