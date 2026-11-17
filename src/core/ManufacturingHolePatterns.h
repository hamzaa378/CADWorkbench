#pragma once
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

// Face-local coordinates in millimetres. Deliberately independent of Qt/OCCT
// so layout validity is testable without the CAD GUI or geometric kernel.
namespace cad::hole_patterns {
struct Point2 {double u=0.0,v=0.0;};
inline bool grid(double centerU,double centerV,double holeRadius,
                 int columns,int rows,double pitchU,double pitchV,
                 double rotationDegrees,std::vector<Point2>* positions,
                 std::string* error=nullptr) {
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!positions)return fail("Hole-pattern destination is missing.");
    if(!std::isfinite(centerU)||!std::isfinite(centerV)||
       !std::isfinite(holeRadius)||holeRadius<1e-5||holeRadius>1e6||
       !std::isfinite(pitchU)||!std::isfinite(pitchV)||
       !std::isfinite(rotationDegrees)||std::abs(rotationDegrees)>180.0||
       columns<1||rows<1||columns>10||rows>10||columns*rows<2)
        return fail("Grid needs 2-100 holes, 1-10 rows and columns, and finite dimensions.");
    if((columns>1&&pitchU<=2*holeRadius+1e-6)||
       (rows>1&&pitchV<=2*holeRadius+1e-6)||
       (columns==1&&pitchU<0)||(rows==1&&pitchV<0)||
       pitchU>1e6||pitchV>1e6)
        return fail("Grid hole pitch must exceed bore diameter on each repeated axis.");
    const double theta=rotationDegrees*std::numbers::pi/180.0;
    const double cosine=std::cos(theta),sine=std::sin(theta);
    std::vector<Point2> result;result.reserve(static_cast<std::size_t>(columns*rows));
    for(int j=0;j<rows;++j)for(int i=0;i<columns;++i){
        const double du=(i-(columns-1)*0.5)*pitchU;
        const double dv=(j-(rows-1)*0.5)*pitchV;
        Point2 at{centerU+du*cosine-dv*sine,centerV+du*sine+dv*cosine};
        if(!std::isfinite(at.u)||!std::isfinite(at.v)||
           std::abs(at.u)>1e9||std::abs(at.v)>1e9)
            return fail("Grid coordinates exceed safe modeling limits.");
        result.push_back(at);
    }
    *positions=std::move(result);if(error)error->clear();return true;
}
inline bool boltCircle(double centerU,double centerV,double holeRadius,
                      int count,double pitchCircleDiameter,double firstAngleDegrees,
                      std::vector<Point2>* positions,std::string* error=nullptr) {
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!positions)return fail("Bolt-circle destination is missing.");
    if(!std::isfinite(centerU)||!std::isfinite(centerV)||
       !std::isfinite(holeRadius)||holeRadius<1e-5||holeRadius>1e6||
       count<2||count>24||!std::isfinite(pitchCircleDiameter)||
       pitchCircleDiameter<=0||pitchCircleDiameter>1e6||
       !std::isfinite(firstAngleDegrees)||std::abs(firstAngleDegrees)>360)
        return fail("Bolt circle requires 2-24 holes, valid PCD and angle.");
    // Closest adjacent centers must be farther apart than the bore diameter.
    const double chord=pitchCircleDiameter*std::sin(std::numbers::pi/count);
    if(chord<=2*holeRadius+1e-6)
        return fail("Bolt-circle holes overlap or touch: increase PCD or reduce diameter.");
    std::vector<Point2> result;result.reserve(static_cast<std::size_t>(count));
    for(int k=0;k<count;++k){
        const double radians=(firstAngleDegrees+k*360.0/count)*std::numbers::pi/180.0;
        Point2 at{centerU+0.5*pitchCircleDiameter*std::cos(radians),
                  centerV+0.5*pitchCircleDiameter*std::sin(radians)};
        if(!std::isfinite(at.u)||!std::isfinite(at.v)||
           std::abs(at.u)>1e9||std::abs(at.v)>1e9)
            return fail("Bolt-circle coordinates exceed safe modeling limits.");
        result.push_back(at);
    }
    *positions=std::move(result);if(error)error->clear();return true;
}
} // namespace cad::hole_patterns
