#include "core/SheetMetalBendSequenceAudit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace cad::sheet;
static int n=0;
static void check(bool v,const char* msg){++n;if(!v)throw std::runtime_error(msg);}
int main(){try{
 // Mixed handedness, dimensions, radii, angle and thickness: 6*6*4
 // independent folded blanks; proves interval conservation and spacing.
 for(int bends=1;bends<=6;++bends)for(int thick=1;thick<=6;++thick)
 for(int seed=0;seed<4;++seed){
   std::vector<double> flanges(bends+1);
   std::vector<BendInput> stations;
   double wanted=0;int reversals=0,prior=0;
   for(int i=0;i<=bends;++i){flanges[i]=10+i*2+seed;wanted+=flanges[i];}
   for(int i=0;i<bends;++i){
      const double degrees=((i+seed)%2?-1.:1.)*(30+i*10);
      const int sign=degrees>0?1:-1;
      if(prior&&prior!=sign)++reversals;
      prior=sign;
      const double radius=2+.5*i;
      stations.push_back({degrees,radius,.25+.1*seed});
      wanted+=std::abs(degrees)*std::numbers::pi/180*(radius+(.25+.1*seed)*thick);
   }
   BendSequenceReport report;std::string err;
   check(inspectBendSequence(flanges,thick,stations,&report,&err),"valid sheet development");
   check(std::abs(report.developedLengthMm-wanted)<1e-8,"conservation of neutral length");
   check(report.reverseDirectionCount==std::size_t(reversals),"bend reversal counted");
   check(report.stations.size()==std::size_t(bends),"all stations emitted");
   check(report.flatFlanges.size()==std::size_t(bends+1),"all flange intervals emitted");
   BendStationSpacingCheck distance;
   check(checkBendStationSpacing(report,0,&distance,&err),"zero limit spacing review");
   check(distance.tooClosePairs.empty(),"positive flange ensures positive spacing");
 }
 std::cout<<n<<" deterministic sheet metal assertions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
