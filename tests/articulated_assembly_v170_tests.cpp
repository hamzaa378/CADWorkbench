#include "core/ArticulatedAssemblyNetwork.h"
#include "core/MotionSweptClearance.h"
#include "core/MotionDriveLimitReview.h"
#include "core/RigidMotionSpeedBound.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cad::articulated;
namespace {
int checks=0;
void check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
Joint make(std::size_t a,std::size_t b,double length,bool driven,double q){
    Joint j;j.parent=a;j.child=b;j.kind=JointKind::Revolute;
    j.axis={0,0,1};j.childAtZero.translation={length,0,0};
    j.minimum=-3.14;j.maximum=3.14;j.initial=q;j.driven=driven;return j;
}
void run(){
    const double pi=std::acos(-1.0);
    std::string reason;Solution out;
    std::vector<Joint> square={make(0,1,10,true,pi/2),make(1,2,10,false,1.45),
                               make(2,3,10,false,1.68),make(3,0,10,false,1.60)};
    for(int i=1;i<4;++i){square[i].minimum=0.8;square[i].maximum=2.5;}
    check(solveNetwork(4,0,square,&out,&reason),"square four-bar closure");
    check(out.poses.size()==4&&out.coordinates.size()==4,"node and joint outputs");
    check(out.maximumClosureErrorMm<1e-4,"full 3D loop closure");
    for(int i=1;i<4;++i)check(std::abs(out.coordinates[i]-pi/2)<1e-4,"joint angle closure");
    check(std::abs(out.coordinates[0]-pi/2)<1e-12,"driven joint unchanged");
    auto snapshot=out;
    square[3].childAtZero.translation.x=60;
    check(!solveNetwork(4,0,square,&out,&reason),"unreachable loop rejected");
    check(out.coordinates==snapshot.coordinates,"failure leaves result unchanged");
    square[3].childAtZero.translation.x=10;
    square[0].driven=false;
    check(!solveNetwork(4,0,square,&out,&reason),"undriven singular four-bar rejected");
    square[0].driven=true;
    square[1].initial=0.4;square[1].minimum=0;square[1].maximum=0.5;
    check(!solveNetwork(4,0,square,&out,&reason),"joint limits enforced");
    square[1].minimum=0.8;square[1].maximum=2.5;square[1].initial=1.45;
    check(solveNetwork(4,0,square,&out,&reason),"restored valid limits");
    auto broken=square;broken[1].axis={0,0,2};
    check(!solveNetwork(4,0,broken,&out,&reason),"nonunit axis rejected");
    broken=square;broken[1].axis={std::numeric_limits<double>::quiet_NaN(),0,1};
    check(!solveNetwork(4,0,broken,&out,&reason),"NaN joint direction rejected");
    broken=square;broken[1].childAtZero.rotation={{-1,0,0,0,1,0,0,0,1}};
    check(!solveNetwork(4,0,broken,&out,&reason),"Mirrored frame rejected before geometry update");
    broken=square;broken[1].parent=10;
    check(!solveNetwork(4,0,broken,&out,&reason),"invalid source rejected");
    broken=square;broken.pop_back();
    check(!solveNetwork(4,0,broken,&out,&reason),"nonclosing free tree rejected");
    broken=square;broken[1].kind=JointKind::Fixed;broken[1].driven=false;
    check(!solveNetwork(4,0,broken,&out,&reason),"free fixed mate rejected");
    // Fully driven rigid-equivalent joint graph: closure is checked without
    // optimizing any coordinate, including an inconsistent final joint.
    for(auto& e:square)e.driven=true;
    square[1].initial=square[2].initial=square[3].initial=pi/2;
    check(solveNetwork(4,0,square,&out,&reason),"all-driven closed loop");
    square[3].initial=pi/2+0.1;
    check(!solveNetwork(4,0,square,&out,&reason),"all-driven closure mismatch");
    // An undriven slider in a closed four-part loop is determined by its
    // neighboring rigid links. The loop fixes its 5 mm displacement uniquely.
    std::vector<Joint> slider(4);
    slider[0].parent=0;slider[0].child=1;
    slider[0].kind=JointKind::Prismatic;slider[0].axis={1,0,0};
    slider[0].childAtZero.translation={5,0,0};
    slider[0].minimum=0;slider[0].maximum=8;
    slider[0].initial=2;slider[0].driven=false;
    slider[1].parent=1;slider[1].child=2;slider[1].childAtZero.translation={0,10,0};
    slider[2].parent=2;slider[2].child=3;slider[2].childAtZero.translation={-10,0,0};
    slider[3].parent=3;slider[3].child=0;slider[3].childAtZero.translation={0,-10,0};
    check(solveNetwork(4,0,slider,&out,&reason),"closed prismatic loop solved");
    check(std::abs(out.coordinates[0]-5)<1e-6,"slider solved to five millimetres");
    check(out.maximumClosureErrorMm<1e-6,"prismatic loop closure residual");
    slider[0].maximum=4;slider[0].initial=2;
    check(!solveNetwork(4,0,slider,&out,&reason),"prismatic joint hard stop enforced");
    check(std::abs(out.coordinates[0]-5)<1e-6,"prismatic failure did not overwrite output");
    slider[0].maximum=8;
    check(solveNetwork(4,2,slider,&out,&reason),"different fixed component supports articulated network");
    check(std::abs(out.poses[2].translation.x)<1e-6,"fixed node remains pinned");
    // Read-only rotating-solid clearance review with a caller-supplied
    // *whole-body* speed envelope, not a guessed speed from pose samples.
    cad::motion::RigidBodySpeedEnvelope moving{{-10,-1,-1,10,1,1},0,0,0,0,0.2};
    cad::motion::RigidBodySpeedEnvelope stationary{{50,-1,-1,52,1,1},51,0,0,0,0};
    cad::motion::RigidPairSpeedBound speedProof;
    check(cad::motion::calculateRigidPairSpeedBound(moving,stationary,&speedProof,&reason),
          "rotating body speed envelope derived");
    check(speedProof.relativeSurfaceSpeedBoundMmPerSecond>2.0,
          "angular velocity contributes to surface speed");
    cad::motion::Settings clearanceSettings;
    clearanceSettings.safetyClearanceMm=2.0;
    cad::motion::Result clearanceReport;
    cad::motion::Evaluate observed=[](double,cad::motion::Observation* item,std::string*){
        item->clearanceMm=20.0;item->overlapVolumeMm3=0.0;return true;};
    check(cad::motion::scanRigidPairWithSpeedBounds({0.0,1.0},clearanceSettings,
          moving,stationary,observed,&clearanceReport,&speedProof,&reason),
          "rigid-body motion study supports rotation speed bounds");
    check(clearanceReport.clearanceBoundSatisfied&&
          clearanceReport.intervalsWithoutClearanceProof==0,
          "conditional speed-bound clearance established");
    const auto previousMotionCount=clearanceReport.observations.size();
    moving.maximumAngularSpeedRadPerSecond=-0.2;
    check(!cad::motion::scanRigidPairWithSpeedBounds({0.0,1.0},clearanceSettings,
          moving,stationary,observed,&clearanceReport,&speedProof,&reason),
          "invalid angular speed fails closed");
    check(clearanceReport.observations.size()==previousMotionCount,
          "motion study output remains unchanged on failure");
    // Sprint 11: import order must not change the physical swept collision
    // witness or silently accept an unsampled collision in a slider fixture.
    cad::proximity::OrderInvariantSweptTracker reindexedSweeps(0.0);
    const cad::proximity::Bounds fixtureBox{5,0,0,6,1,1};
    const cad::proximity::Bounds beforeBox{0,0,0,1,1,1};
    const cad::proximity::Bounds afterBox{10,0,0,11,1,1};
    check(reindexedSweeps.observe(0.0,{{"slider",beforeBox},{"fixture",fixtureBox}},&reason),
          "unordered imported slider pose");
    check(reindexedSweeps.observe(1.0,{{"fixture",fixtureBox},{"slider",afterBox}},&reason),
          "reordered imported slider pose");
    check(reindexedSweeps.pairs().size()==1 &&
          reindexedSweeps.pairs()[0].contactSegments==1,
          "reordered bodies preserve unsampled fixture collision");
    check(!reindexedSweeps.observe(1.0,{{"fixture",fixtureBox},{"slider",afterBox}},&reason),
          "out-of-order sample rejected");
    check(reindexedSweeps.sampleCount()==2,
          "invalid pose cannot modify swept collision history");
    // Backend v3: produce a bounded quintic drive schedule without mutating
    // an assembly. A separate solver/collision run must validate its poses.
    cad::joint_motion::PlannedQuinticMotion motionPlan;
    check(cad::joint_motion::planQuinticMotion({{1,0,10,0,10}},
          {{1,2,4}},21,&motionPlan,&reason),"quintic motion schedule");
    check(motionPlan.durationSeconds>=9.375,
          "quintic minimum duration respects speed bound");
    check(motionPlan.samples.size()==21&&
          std::abs(motionPlan.samples[10].coordinates[0].second-5)<1e-10,
          "quintic motion coordinate samples");

}
}
int main(){try{run();std::cout<<checks<<" articulated network checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
