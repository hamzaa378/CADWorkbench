#pragma once
#include "PolygonFeatureProfile.h"
#include <string>
#include <vector>

namespace cad::advancedprofile {
struct Point3 { double x=0, y=0, z=0; };
struct LoftSection { double z=0; std::vector<polygon::Point> points; };
// Validate sources BEFORE invoking OpenCascade. All input dimensions are mm;
// polygons have implicit closing edges. These helpers require no Qt or OCCT.
bool validateSweep(const std::vector<polygon::Point>& profile,
                   const std::vector<Point3>& path,std::string* issue=nullptr);
bool validateMultiLoft(const std::vector<LoftSection>& sections,std::string* issue=nullptr);
bool validateAxisRevolve(const std::vector<polygon::Point>& profile,
                         const Point3& origin,const Point3& direction,
                         double angleDegrees,std::string* issue=nullptr);
}
