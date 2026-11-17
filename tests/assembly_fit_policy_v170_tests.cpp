#include "core/AssemblyFitPolicy.h"
#include "core/CircularEdgeLineage.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace cad::fit;
void check(bool value){if(!value)throw std::runtime_error("Assembly fit classification failed");}
int main(){
    try{
        Status result=Status::Clear;
        check(classify(10,0,5,&result)&&result==Status::Clear);
        check(classify(4,0,5,&result)&&result==Status::InsufficientClearance);
        check(classify(0,0,5,&result)&&result==Status::Contact);
        check(classify(0,10,5,&result)&&result==Status::Interference);
        check(classify(0,0,0,&result)&&result==Status::Contact);
        check(classify(1e-6,0,0,&result)&&result==Status::Clear);
        check(!classify(-1,0,5,&result));
        check(!classify(1,-1,5,&result));
        check(!classify(1,0,-1,&result));
        check(!classify(1,0,std::numeric_limits<double>::infinity(),&result));
        check(!classify(std::numeric_limits<double>::quiet_NaN(),0,5,&result));
        check(!classify(1,0,5,nullptr));
        for(int i=0;i<500;++i){
            const double distance=static_cast<double>(i)/10.0;
            check(classify(distance,0,20,&result));
            check(result==(distance<=1e-7?Status::Contact:
                distance+1e-8<20?Status::InsufficientClearance:Status::Clear));
        }
        ToleranceResult assessed;
        check(assessTolerance(8,0,5,1,1,&assessed));
        check(assessed.status==ToleranceStatus::GuaranteedClearance);
        check(assessed.lowerBoundGapMm==6&&assessed.signedClearanceMarginMm==1);
        check(assessTolerance(8,0,5,2,2,&assessed));
        check(assessed.status==ToleranceStatus::BelowRequiredClearance);
        check(assessed.lowerBoundGapMm==4&&assessed.signedClearanceMarginMm==-1);
        check(assessTolerance(8,0,5,4,4,&assessed));
        check(assessed.status==ToleranceStatus::NoGuaranteedSeparation);
        check(assessed.lowerBoundGapMm==0&&assessed.signedClearanceMarginMm==-5);
        check(assessTolerance(8,0,8,0,0,&assessed));
        check(assessed.status==ToleranceStatus::GuaranteedClearance);
        check(assessTolerance(0,0,0,0,0,&assessed));
        check(assessed.status==ToleranceStatus::NoGuaranteedSeparation);
        check(assessTolerance(8,800,5,0,0,&assessed));
        check(assessed.status==ToleranceStatus::NominalInterference);
        check(assessed.lowerBoundGapMm==0);
        check(!assessTolerance(8,0,5,-1,0,&assessed));
        check(!assessTolerance(8,0,5,0,std::numeric_limits<double>::infinity(),&assessed));
        check(!assessTolerance(8,0,5,0,std::numeric_limits<double>::quiet_NaN(),&assessed));
        check(!assessTolerance(8,0,5,0,0,nullptr));
        check(!assessTolerance(-2,0,5,0,0,&assessed));
        check(assessed.status==ToleranceStatus::NominalInterference);
        // Safety property: a larger outward deviation can never improve the
        // conservative clearance status or minimum clearance bound.
        for(int i=1;i<=2000;++i){
            const double gap=static_cast<double>(i)/100.0;
            const double dev=static_cast<double>(i%120)/50.0;
            ToleranceResult a,b;
            check(assessTolerance(gap,0,1.5,dev,0.0,&a));
            check(assessTolerance(gap,0,1.5,dev,0.25,&b));
            check(b.lowerBoundGapMm<=a.lowerBoundGapMm+1e-10);
            check(b.signedClearanceMarginMm<=a.signedClearanceMarginMm+1e-10);
            if(b.status==ToleranceStatus::GuaranteedClearance)
                check(a.status==ToleranceStatus::GuaranteedClearance);
        }
        check(std::string(toleranceStatusName(ToleranceStatus::GuaranteedClearance))==
              "Clearance guaranteed within bounds");
        // Safe core of the drawing-only circular witness recovery: retain a
        // concentric, coaxial bore after bounded diameter changes, but never
        // accept moved axes or arbitrary radius jumps.
        const cad::circular_lineage::Reference saved{{1,2,3},{0,0,1},10};
        check(cad::circular_lineage::compatible(saved,{{1,2,3},{0,0,1},11}));
        check(!cad::circular_lineage::compatible(saved,{{1.1,2,3},{0,0,1},11}));
        check(!cad::circular_lineage::compatible(saved,{{1,2,3},{0,0,1},12.5}));
        std::cout<<"512 nominal fit checks plus 2,000 tolerance monotonicity checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
