#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace cad::polygon {
struct Point { double x=0.0, y=0.0; };
// Closed, straight-line profile; the closing edge is implicit.
// No holes, curves or zero-length edges. In a revolve profile, x is radius
// and y is axial height in the world XZ half-plane.
bool validate(const std::vector<Point>& points, bool radial, std::string* issue=nullptr);
bool validateLoft(const std::vector<Point>& lower,const std::vector<Point>& upper,
                  double height,std::string* issue=nullptr);
double signedArea(const std::vector<Point>& points);
}
