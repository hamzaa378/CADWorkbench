#include "core/DrillingFeasibility.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cad::drilling;
namespace {
int checks=0;
void check(bool value,const char* what){++checks;if(!value)throw std::runtime_error(what);}
Setup setup(){Setup s;s.bodyId="body-7";s.geometryRevision="rev-21";s.stockTopZ=20;s.stockBottomZ=0;s.clearanceZ=25;return s;}
std::vector<Tool> tools() {return {{"DRILL-5",5,30},{"DRILL-8",8,30},{"DRILL-5-SHORT",5,3}};}
void run(){
    const std::vector<Hole> holes={{"H2",50,0,5,20,8,false},{"H1",10,0,5,20,2,false},
       {"H3",25,0,8,20,0,true},{"H4",70,0,9,20,10,false}};
    Plan out;std::string err;
    check(plan(setup(),holes,tools(),&out,&err),"Basic feasible plan rejected");
    check(err.empty(),"Successful plan retained error");
    check(out.eligible==2&&out.conditional==1&&out.rejected==1,"Wrong approval classes");
    check(out.rows[1].visitOrder==1&&out.rows[2].visitOrder==2&&out.rows[0].visitOrder==3,"Wrong travel ordering");
    check(std::abs(out.estimatedXYTravel-50)<1e-9,"Wrong preview XY travel");
    check(out.rows[0].toolId=="DRILL-5"&&out.rows[3].visitOrder==0,"Wrong tool matching");
    std::string csv;
    check(exportReviewCsv(out,&csv,&err),"CSV review failed");
    check(csv.find("NOT machine instructions")!=std::string::npos&&
          csv.find("rev-21")!=std::string::npos,"Revision or disclaimer missing");
    check(csv.find("FIXTURE REVIEW")!=std::string::npos,"Fixture caveat missing");
    auto before=out;auto badSetup=setup();badSetup.clearanceZ=19;
    check(!plan(badSetup,holes,tools(),&out,&err),"Unsafe clearance accepted");
    check(out.rows[0].toolId==before.rows[0].toolId,"Invalid plan mutated output");
    badSetup=setup();badSetup.diameterTolerance=-1;
    check(!plan(badSetup,holes,tools(),&out,&err),"Negative tolerance accepted");
    badSetup=setup();badSetup.geometryRevision.clear();
    check(!plan(badSetup,holes,tools(),&out,&err),"Unversioned plan accepted");
    auto badTools=tools();badTools[1].id=badTools[0].id;
    check(!plan(setup(),holes,badTools,&out,&err),"Duplicate cutter accepted");
    badTools=tools();badTools[0].usableLength=std::nan("");
    check(!plan(setup(),holes,badTools,&out,&err),"Nonfinite cutter accepted");
    auto badHole=holes;badHole[1].label=badHole[0].label;
    check(!plan(setup(),badHole,tools(),&out,&err),"Duplicate hole accepted");
    badHole=holes;badHole[1].topZ=18;
    check(plan(setup(),badHole,tools(),&out,&err),"Invalid hole should produce rejected review row");
    check(out.rows[1].decision==Decision::Rejected,"Buried hole was accepted");
    badHole=holes;badHole[0].diameter=9;
    check(plan(setup(),badHole,tools(),&out,&err)&&out.rows[0].decision==Decision::Rejected,"Oversize drill mismatch accepted");
    auto fixture=setup();fixture.fixtureExitReviewed=true;
    check(plan(fixture,holes,tools(),&out,&err)&&out.rows[2].decision==Decision::ConditionalFixtureReview,
          "Fixture check incorrectly approved through bore");
    check(!plan(setup(),holes,tools(),nullptr,&err),"Null output accepted");
    badHole=holes;badHole[0].label="=cmd|' /C calc'!A0";
    check(plan(setup(),badHole,tools(),&out,&err)&&exportReviewCsv(out,&csv,&err),"Formula-guard test failed");
    check(csv.find("\"'=cmd")!=std::string::npos,"CSV formula injection not guarded");
    badHole=holes;badHole[0].label="bad\ncontrol";
    check(!plan(setup(),badHole,tools(),&out,&err),"Control characters accepted");
    auto zero=setup();zero.stockBottomZ=zero.stockTopZ;
    check(!plan(zero,holes,tools(),&out,&err),"Invalid stock depth accepted");
    for(int i=0;i<256;++i){
        auto cyclic=holes;cyclic[0].x=10+i;
        check(plan(setup(),cyclic,tools(),&out,&err),"Randomized stable plan failed");
        check(out.eligible+out.conditional+out.rejected==4,"Plan class accounting incorrect");
    }
}
}
int main(){try{run();std::cout<<"Drilling feasibility: "<<checks<<" checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
