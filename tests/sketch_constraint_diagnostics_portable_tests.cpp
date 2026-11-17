#include "core/SketchConstraintDiagnostics.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <vector>
namespace {
int tests=0;
void check(bool test,const char* why){++tests;if(!test)throw std::runtime_error(why);}
void run(){
    using cad::sketchdiagnostics::findRedundantConstraints;
    std::vector<int> redundant={7};std::string error;
    const std::vector<std::vector<double>> independent={{1,0,0},{0,1,0},{0,0,1}};
    check(findRedundantConstraints(independent,{1,1,1},&redundant,&error),"valid 3-DOF Jacobian");
    check(redundant.empty(),"independent constraints flagged redundant");
    // A 2-D sketch point with one x constraint can still move in y.
    std::vector<int> mobile={99};
    check(findRedundantConstraints({{1,0}},{1},&redundant,&error,&mobile),
          "local coordinate mobility computed");
    check(mobile==std::vector<int>{1},"y remains mobile after locking x");
    check(findRedundantConstraints({{1,1}},{1},&redundant,&error,&mobile),
          "coupled local mobility computed");
    check(mobile==std::vector<int>({0,1}),
          "both coordinates participate in one coupled DOF");
    check(findRedundantConstraints({{1,0},{0,1}},{1,1},&redundant,&error,&mobile),
          "fully constrained local mobility computed");
    check(mobile.empty(),"fully locked point has no free coordinates");
    check(findRedundantConstraints({{0,0}},{1},&redundant,&error,&mobile),
          "zero gradient mobility computed");
    check(mobile==std::vector<int>({0,1}),"zero row leaves both coordinates mobile");
    check(findRedundantConstraints({{1,0,0},{0,1,0},{1,0,0}},{1,1,1},&redundant,&error),
          "duplicate constraint Jacobian");
    check(redundant==std::vector<int>{2},"duplicate should be identified as third constraint");
    check(findRedundantConstraints({{1,0},{0,1},{1,0}},{2,1},&redundant,&error),"two-equation block");
    check(redundant==std::vector<int>{1},"duplicate block not identified");
    check(findRedundantConstraints({{1,0},{1,0},{0,1}},{2,1},&redundant,&error),"partially redundant block");
    check(redundant.empty(),"partially useful block incorrectly called redundant");
    check(findRedundantConstraints({{1e-3,0},{1e6,0}},{1,1},&redundant,&error),
          "mixed-scale rows");
    check(redundant==std::vector<int>{1},"mixed-unit duplicate missed");
    check(findRedundantConstraints({{0,0},{0,1}},{1,1},&redundant,&error),"zero Jacobian row");
    check(redundant==std::vector<int>{0},"zero row not diagnosed");
    const auto old=redundant;
    const auto oldMobile=mobile;
    check(!findRedundantConstraints({{1,0}},{2},&redundant,&error,&mobile),"mismatched row counts accepted");
    check(redundant==old&&mobile==oldMobile,"malformed input overwrote diagnostic result");
    check(!findRedundantConstraints({{1,0},{0}},{1,1},&redundant,&error),"jagged Jacobian accepted");
    check(!findRedundantConstraints({{std::numeric_limits<double>::quiet_NaN(),1}},{1},
                                    &redundant,&error),"NaN Jacobian accepted");
    check(!findRedundantConstraints({{1}},{1},nullptr,&error),"null output accepted");
}
}
int main(){try{run();std::cout<<"PASS: "<<tests<<" sketch rank diagnostics checks\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL "<<tests<<": "<<e.what()<<'\n';return 1;}}
